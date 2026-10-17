import { linkAbortSignals } from '../services/abort-signals';
import type { Translate } from '../i18n';

type Activity = 'update' | 'music' | 'package' | 'language';
interface BlockingDownload {
  label: string;
  cancel(): Promise<void>;
}

/** 启动准备拥有自己的取消信号。过期任务不能修改当前下载状态。 */
export function createPreparationActivity({
  translate,
  onChange,
}: {
  translate: Translate;
  onChange(finished: boolean): void;
}) {
  let current: AbortController | null = null;
  let update: AbortController | null = null;
  let music: AbortController | null = null;
  const active: Record<Activity, boolean> = {
    update: false,
    music: false,
    package: false,
    language: false,
  };
  function cancellationLabel(fallback: string | null = null) {
    return active.update
      ? translate('package.cancelUpdate')
      : active.music
        ? translate('music.cancelDownload')
        : active.package || active.language
          ? translate('package.cancelDownload')
          : fallback;
  }
  function begin(externalSignal?: AbortSignal) {
    const controller = new AbortController();
    const linked = linkAbortSignals(controller.signal, externalSignal);
    const updateController = new AbortController();
    const musicController = new AbortController();
    current = controller;
    update = updateController;
    music = musicController;
    const isCurrent = () => current === controller;
    return {
      controller,
      signal: linked.signal,
      updateSignal: updateController.signal,
      musicSignal: musicController.signal,
      isCurrent,
      setActivity(kind: Activity, value: boolean) {
        if (!isCurrent()) return;
        active[kind] = value;
        onChange(false);
      },
      runtimePlanned() {
        if (!isCurrent()) return;
        active.update = false;
      },
      finish() {
        linked.dispose();
        if (!isCurrent()) return;
        current = update = music = null;
        active.update = active.music = active.package = active.language = false;
        onChange(true);
      },
    };
  }
  return {
    begin,
    hasLaunch: () => current !== null,
    isBlocking: () => Object.values(active).some(Boolean),
    cancellationLabel,
    abort(reason?: unknown) {
      current?.abort(reason);
    },
    async cancelDownload(fallback: BlockingDownload | null) {
      if (active.update) update?.abort();
      else if (active.music) music?.abort();
      else if (active.package || active.language)
        current?.abort(new DOMException('已取消下载', 'AbortError'));
      else if (fallback) await fallback.cancel();
      else current?.abort(new DOMException('已取消下载', 'AbortError'));
    },
  };
}
