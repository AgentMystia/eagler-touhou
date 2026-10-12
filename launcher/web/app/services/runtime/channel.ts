import { HOST_PROTOCOL, type GameId } from '@eagler/contracts/product-catalog';
import type {
  RuntimeProtocolCommand,
  RuntimeCommandPayloads,
  RuntimeResponseMessage,
} from '@eagler/contracts/runtime-protocol';
import type { RuntimeSessionToken } from '../../../src/launcher/runtime-session.mts';
import type { Translate } from '../../i18n';
import type { RuntimeSnapshot, RuntimeTimers, RuntimeWindow, Timer } from './types';
import { RuntimeOperationError, RuntimeWaitTimeoutError } from './errors';
interface RuntimeChannelContext {
  token: RuntimeSessionToken;
  runtime: RuntimeWindow;
  game: GameId;
  origin: string;
}
interface PendingRequest {
  token: RuntimeSessionToken;
  resolve(response: RuntimeResponseMessage): void;
  reject(error: unknown): void;
  timer: Timer;
  progress?: (mode: string, loaded: number) => void;
}
interface Waiter {
  token: RuntimeSessionToken;
  kind: 'ready' | 'first-frame';
  resolve(): void;
  reject(error: unknown): void;
  timer: Timer;
}

/** Owns outstanding RPCs and readiness waits. Only authenticated replies enter
 * accept(); retiring an epoch rejects its outstanding work in one place. */
export function createRuntimeChannel({
  timers,
  translate,
  commandTimeout = 15_000,
  currentContext,
  getSnapshot,
  isCurrent,
  assertCurrent,
}: {
  timers: RuntimeTimers;
  translate: Translate;
  commandTimeout?: number;
  currentContext(): RuntimeChannelContext;
  getSnapshot(): Pick<RuntimeSnapshot, 'ready' | 'firstFrame' | 'generationId'>;
  isCurrent(token: RuntimeSessionToken): boolean;
  assertCurrent(token: RuntimeSessionToken): void;
}) {
  const pending = new Map<string, PendingRequest>();
  const waiters = new Set<Waiter>();
  let requestSerial = 0;
  function waitFor(
    kind: Waiter['kind'],
    token: RuntimeSessionToken,
    timeout: number,
  ): Promise<void> {
    assertCurrent(token);
    if (kind === 'ready' ? getSnapshot().ready : getSnapshot().firstFrame) return Promise.resolve();
    return new Promise((resolve, reject) => {
      const waiter: Waiter = {
        token,
        kind,
        resolve,
        reject,
        timer: timers.setTimeout(() => {
          waiters.delete(waiter);
          reject(
            new RuntimeWaitTimeoutError(
              kind,
              translate(
                kind === 'first-frame'
                  ? 'runtime.firstFrameLate'
                  : getSnapshot().generationId
                    ? 'runtime.localLoadTimeout'
                    : 'runtime.gameLoadTimeout',
              ),
            ),
          );
        }, timeout),
      };
      waiters.add(waiter);
    });
  }
  function settle(kind: Waiter['kind']) {
    for (const waiter of waiters)
      if (waiter.kind === kind && isCurrent(waiter.token)) {
        waiters.delete(waiter);
        timers.clearTimeout(waiter.timer);
        waiter.resolve();
      }
  }
  function send<C extends RuntimeProtocolCommand>(
    command: C,
    payload: RuntimeCommandPayloads[C],
    timeout = commandTimeout,
  ): Promise<RuntimeResponseMessage> {
    let context: RuntimeChannelContext;
    try {
      context = currentContext();
    } catch (error) {
      return Promise.reject(error);
    }
    const { token, runtime, game, origin } = context;
    const request = `runtime-${token.id}-${++requestSerial}`;
    return new Promise((resolve, reject) => {
      const expire = () => {
        pending.delete(request);
        reject(new Error(translate('runtime.operationTimeout', { command })));
      };
      const item: PendingRequest = {
        token,
        resolve,
        reject,
        timer: timers.setTimeout(expire, timeout),
      };
      if (command === 'configure') {
        const loaded = new Map<string, number>();
        item.progress = (mode, bytes) => {
          if (!Number.isFinite(bytes) || bytes <= (loaded.get(mode) ?? 0)) return;
          loaded.set(mode, bytes);
          timers.clearTimeout(item.timer);
          item.timer = timers.setTimeout(expire, timeout);
        };
      }
      pending.set(request, item);
      try {
        // Envelope fields always win, including over untyped configure payloads.
        runtime.postMessage(
          { ...payload, protocol: HOST_PROTOCOL, game, epoch: token.id, command, request },
          origin,
        );
      } catch (error) {
        timers.clearTimeout(item.timer);
        pending.delete(request);
        reject(error);
      }
    });
  }

  function rejectWaiters(error: unknown) {
    for (const waiter of waiters) {
      timers.clearTimeout(waiter.timer);
      waiter.reject(error);
    }
    waiters.clear();
  }
  function rejectAll(error: unknown) {
    for (const item of pending.values()) {
      timers.clearTimeout(item.timer);
      item.reject(error);
    }
    pending.clear();
    rejectWaiters(error);
  }
  function accept(message: RuntimeResponseMessage) {
    const item = pending.get(message.request);
    if (!item || !isCurrent(item.token)) return;
    pending.delete(message.request);
    timers.clearTimeout(item.timer);
    if (message.ok) item.resolve(message);
    else {
      const error = new RuntimeOperationError(
        typeof message.error === 'string' ? message.error : translate('runtime.operationFailed'),
      );
      if (Number.isInteger(message.errno)) error.errno = Number(message.errno);
      item.reject(error);
    }
  }
  function progress(mode: string, loaded: number) {
    for (const item of pending.values()) item.progress?.(mode, loaded);
  }
  return { send, waitFor, settle, accept, progress, rejectAll, rejectWaiters };
}
