import type { GameId } from '@eagler/contracts/product-catalog';
import type { RuntimeCommandPayloads } from '@eagler/contracts/runtime-protocol';
import type { RuntimeSessionToken } from '../../../src/launcher/runtime-session.mts';
import { isReplayFilePath, isSafeReplayArchivePath } from '../../../src/launcher/replay-files.mts';
import type { createRuntimeChannel } from './channel';
import type {
  RuntimeFileCommand,
  RuntimeFileSession,
  RuntimeSnapshot,
  RuntimePlan,
  RuntimeDependencies,
  RuntimeTimers,
  Timer,
} from './types';
import { RuntimeSessionSupersededError } from './errors';

export interface FileSessionOwner {
  token: RuntimeSessionToken;
  promise: Promise<unknown>;
  invalidated: boolean;
  restarting: boolean;
  invalidate(): void;
}
interface FileSessionState {
  token: RuntimeSessionToken | null;
  snapshot: RuntimeSnapshot;
  plan: RuntimePlan | null;
  preflight: boolean;
  disposed: boolean;
  closing: boolean;
  launchRequested: boolean;
}
interface FileSessionPorts {
  state(): FileSessionState;
  assertCurrent(token: RuntimeSessionToken): void;
  send: ReturnType<typeof createRuntimeChannel>['send'];
  update(patch: Partial<RuntimeSnapshot>): void;
  retire(owner: FileSessionOwner): boolean;
  prepare(plan: RuntimePlan, owner: FileSessionOwner): Promise<unknown>;
  retainGeneration: RuntimeDependencies['retainGeneration'];
  releaseGeneration: RuntimeDependencies['releaseGeneration'];
  timers: RuntimeTimers;
  leaseTimeout: number;
  warn(error: unknown): void;
}

/** 文件事务独占访问；重启时继续持有同一事务并衔接 Package 租约。 */
export function createRuntimeFileSessions({
  state,
  assertCurrent,
  send,
  update,
  retire,
  prepare,
  retainGeneration,
  releaseGeneration,
  timers,
  leaseTimeout,
  warn,
}: FileSessionPorts) {
  let fileSession: FileSessionOwner | null = null;
  /** Acquire synchronously so a same-tick Start cannot race file decoding or writes.
   * A terminal Runtime event invalidates callbacks even while local file I/O waits. */
  function run<T>(
    game: GameId,
    operation: (access: RuntimeFileSession) => Promise<T>,
    {
      readOnly = false,
      replayMutation = false,
      runtimeVariant,
      epoch,
    }: {
      readOnly?: boolean;
      replayMutation?: boolean;
      runtimeVariant?: 'normal' | 'multiplayer';
      epoch?: number;
    } = {},
  ): Promise<T> {
    const { token, snapshot, preflight, disposed, closing, launchRequested } = state();
    const readableRunning =
      (readOnly || replayMutation) && snapshot.phase === 'running' && snapshot.launched;
    if (
      !token ||
      preflight ||
      disposed ||
      closing ||
      fileSession ||
      (!readableRunning &&
        (launchRequested || snapshot.launched || snapshot.phase !== 'prepared')) ||
      snapshot.game !== game ||
      !snapshot.ready ||
      snapshot.saveUnavailable ||
      (runtimeVariant !== undefined && snapshot.runtimeVariant !== runtimeVariant) ||
      (epoch !== undefined && token.id !== epoch)
    ) {
      return Promise.reject(
        new Error('Prepare this game and stop playback before managing its files'),
      );
    }
    let invalidate!: () => void;
    const invalidated = new Promise<never>((_resolve, reject) => {
      invalidate = () => reject(new RuntimeSessionSupersededError());
    });
    const session: FileSessionOwner = {
      token,
      invalidated: false,
      restarting: false,
      invalidate() {
        session.invalidated = true;
        invalidate();
      },
      promise: Promise.resolve(),
    };
    const assertOwner = () => {
      if (state().disposed || session.invalidated || fileSession !== session)
        throw new RuntimeSessionSupersededError();
    };
    function fileAccess(currentToken: RuntimeSessionToken): RuntimeFileSession {
      const assertAccess = () => {
        assertOwner();
        assertCurrent(currentToken);
        const { snapshot, launchRequested } = state();
        const allowed =
          (snapshot.phase === 'prepared' && !launchRequested && !snapshot.launched) ||
          ((readOnly || replayMutation) && snapshot.phase === 'running' && snapshot.launched);
        if (session.token !== currentToken || session.restarting || !allowed)
          throw new RuntimeSessionSupersededError();
      };
      return Object.freeze({
        epoch: currentToken.id,
        async send<C extends RuntimeFileCommand>(command: C, payload: RuntimeCommandPayloads[C]) {
          assertAccess();
          if (!['list', 'read', 'write', 'remove'].includes(command))
            throw new Error('Invalid Runtime file command');
          if (readOnly && !['list', 'read'].includes(command))
            throw new Error('Read-only Runtime file sessions cannot mutate files');
          if (replayMutation && ['write', 'remove'].includes(command)) {
            const path = (payload as { path?: unknown }).path;
            if (!isReplayFilePath(path) || !isSafeReplayArchivePath(path))
              throw new Error('A Replay mutation cannot change save, config or Hint files');
          }
          const response = await send(command, payload);
          assertAccess();
          return response;
        },
        async sync() {
          assertAccess();
          await send('sync', {}, 10_000);
          assertAccess();
          update({ saveError: null });
        },
        async retire() {
          assertAccess();
          if (readOnly) throw new Error('Read-only Runtime file sessions cannot retire the game');
          if (!retire(session))
            throw new Error(
              state().snapshot.closeError ?? 'Could not retire the imported-file Runtime',
            );
        },
        async restart({ sync: flush = true } = {}) {
          assertAccess();
          if (readOnly) throw new Error('Read-only Runtime file sessions cannot restart the game');
          const plan = state().plan;
          if (!plan || plan.game !== game) throw new RuntimeSessionSupersededError();
          session.restarting = true;
          // A separate Package lease bridges retirement's release and the new
          // owner's retain, including asynchronous code/resource preparation.
          const pin = `runtime-restart-${game}-${currentToken.id}-${Math.random().toString(36).slice(2)}`;
          let pinAcquired = false,
            pinActive = false,
            pinTimer: Timer | null = null;
          try {
            if (flush) await send('sync', {}, 10_000);
            assertOwner();
            assertCurrent(currentToken);
            const pinnedGeneration = plan.generation;
            if (pinnedGeneration) {
              await retainGeneration(game, pinnedGeneration.id, { leaseId: pin });
              pinAcquired = true;
              assertOwner();
              assertCurrent(currentToken);
              pinActive = true;
              pinTimer = timers.setInterval(() => {
                if (!pinActive) return;
                void retainGeneration(game, pinnedGeneration.id, { leaseId: pin })
                  .then(() => {
                    if (!pinActive) return releaseGeneration(pin);
                  })
                  .catch(warn);
              }, leaseTimeout);
            }
            if (!retire(session))
              throw new Error(
                state().snapshot.closeError ?? 'Could not retire Runtime for save verification',
              );
            assertOwner();
            await prepare(plan, session);
            assertOwner();
            const next = state().token;
            if (!next || next.id === currentToken.id || next.game !== game)
              throw new RuntimeSessionSupersededError();
            assertCurrent(next);
            session.token = next;
            session.restarting = false;
            return fileAccess(next);
          } finally {
            pinActive = false;
            if (pinTimer !== null) timers.clearInterval(pinTimer);
            if (pinAcquired) await releaseGeneration(pin).catch(warn);
            session.restarting = false;
          }
        },
      });
    }
    const access = fileAccess(token);
    fileSession = session;
    session.promise = Promise.race([
      Promise.resolve().then(() => {
        assertOwner();
        assertCurrent(token);
        return operation(access);
      }),
      invalidated,
    ]).finally(() => {
      if (fileSession === session) {
        fileSession = null;
        update({ fileOperationBusy: false });
      }
    });
    update({ fileOperationBusy: true });
    return session.promise as Promise<T>;
  }
  return {
    run,
    current: () => fileSession,
    invalidate() {
      fileSession?.invalidate();
    },
  };
}
