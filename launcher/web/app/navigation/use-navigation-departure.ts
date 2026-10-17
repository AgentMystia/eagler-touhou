import { useEffect, useRef } from 'react';
import type { Blocker } from 'react-router';
import type { Surface } from './surface-state';

interface RuntimeDeparture {
  manualRoomExit: boolean;
  titleOverlayWasOpen: boolean;
}
interface DepartureOptions {
  blocker: Blocker;
  reason: 'touch' | 'runtime' | 'room' | 'page' | null;
  surface: Surface;
  requestRuntimeClose?(): Promise<boolean>;
  requestRoomLeave?(): void;
  returnToPageParent(): void;
  captureRuntimeClose(): RuntimeDeparture;
  resetRuntimeDeparture(): void;
  resetRoomDeparture(): void;
  completeRuntimeDeparture(
    blocker: Extract<Blocker, { state: 'blocked' }>,
    context: RuntimeDeparture,
  ): void;
}

/** 异步离开只确认原目标；关闭游戏的结果不能成为后续导航的通行证。 */
export function useNavigationDeparture(options: DepartureOptions) {
  const { blocker, reason, requestRuntimeClose, requestRoomLeave, surface } = options;
  const latestBlocker = useRef(blocker);
  latestBlocker.current = blocker;
  const roomLeaveTaskKey = useRef<string | null>(null);
  const runtimeCloseTask = useRef<{
    destination: string;
    settled: boolean;
    context: RuntimeDeparture;
  } | null>(null);
  const mounted = useRef(true);
  useEffect(() => {
    mounted.current = true;
    return () => {
      mounted.current = false;
    };
  }, []);
  useEffect(() => {
    if (blocker.state !== 'blocked' || reason !== 'page') return;
    blocker.reset();
    options.returnToPageParent();
  }, [blocker]);
  useEffect(() => {
    if (blocker.state !== 'blocked') {
      if (runtimeCloseTask.current?.settled) runtimeCloseTask.current = null;
      return;
    }
    if (reason !== 'runtime' || runtimeCloseTask.current) return;
    const task = {
      destination: blocker.location.key,
      settled: false,
      context: options.captureRuntimeClose(),
    };
    runtimeCloseTask.current = task;
    void Promise.resolve()
      .then(() => (mounted.current ? (requestRuntimeClose?.() ?? false) : false))
      .catch(() => false)
      .then((closed) => {
        if (!mounted.current || runtimeCloseTask.current !== task) return;
        task.settled = true;
        const current = latestBlocker.current;
        if (current.state !== 'blocked') {
          runtimeCloseTask.current = null;
          return;
        }
        if (current.location.key !== task.destination || !closed) {
          options.resetRuntimeDeparture();
          current.reset();
          return;
        }
        options.completeRuntimeDeparture(current, task.context);
      });
  }, [blocker, requestRuntimeClose, surface]);
  useEffect(() => {
    if (blocker.state === 'unblocked') roomLeaveTaskKey.current = null;
    if (
      blocker.state !== 'blocked' ||
      reason !== 'room' ||
      roomLeaveTaskKey.current === blocker.location.key
    )
      return;
    roomLeaveTaskKey.current = blocker.location.key;
    if (requestRoomLeave) requestRoomLeave();
    else {
      options.resetRoomDeparture();
      blocker.reset();
    }
  }, [blocker, requestRoomLeave]);
}
