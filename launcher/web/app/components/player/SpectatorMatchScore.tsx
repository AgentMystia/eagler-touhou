import { useLayoutEffect, useState } from 'react';
import { createPortal } from 'react-dom';
import { useLocale } from '../../i18n';

/** The native playback bar owns its layout and cursor. Room wins keep their
 * existing React owner and render into the bar's score slot. */
export function SpectatorMatchScore({
  wins,
  names = ['', ''],
}: {
  wins: readonly [number, number];
  names?: readonly [string, string];
}) {
  const { locale } = useLocale();
  const [target, setTarget] = useState<HTMLElement | null>(null);
  useLayoutEffect(() => {
    const frame = document.getElementById('gameFrame') as HTMLIFrameElement | null;
    if (!frame) return;
    let observer: MutationObserver | null = null;
    const bind = () => {
      observer?.disconnect();
      try {
        const body = frame.contentDocument?.body;
        const find = () => setTarget(body?.querySelector<HTMLElement>('#spectatorScore') ?? null);
        if (body) {
          observer = new MutationObserver(find);
          observer.observe(body, { childList: true });
        }
        find();
      } catch {
        setTarget(null);
      }
    };
    frame.addEventListener('load', bind);
    bind();
    return () => {
      observer?.disconnect();
      frame.removeEventListener('load', bind);
    };
  }, []);
  useLayoutEffect(() => {
    if (!target) return;
    const native = target.ownerDocument;
    native.documentElement.lang = locale;
    const panel = native.getElementById('spectatorTimeline');
    const icons: Record<string, string> = {
      pause: 'player-pause.svg',
      play: 'player-play.svg',
      live: 'player-live.svg',
      toggle: 'room-caret-right.svg',
    };
    panel?.querySelectorAll<HTMLImageElement>('[data-icon]').forEach((icon) => {
      const path = icons[icon.dataset.icon || ''];
      if (path) icon.src = new URL(`assets/${path}`, document.baseURI).href;
    });
    panel?.style.setProperty(
      '--ui-font',
      getComputedStyle(document.documentElement).getPropertyValue('--ui-font'),
    );
    // Copy only the page's existing font faces. Loading the whole launcher CSS
    // in the game frame would alter its canvas/dialog layout.
    const fonts = native.createElement('style');
    fonts.id = 'spectatorUiFonts';
    const faces: string[] = [];
    for (const sheet of Array.from(document.styleSheets)) {
      try {
        for (const rule of Array.from(sheet.cssRules)) {
          if (rule.type !== 5 || !/ET (?:Yatra|Chill Round)/.test(rule.cssText)) continue;
          faces.push(
            rule.cssText.replace(
              /url\(["']?([^"')]+)["']?\)/g,
              (_, path) => `url("${new URL(path, sheet.href || document.baseURI).href}")`,
            ),
          );
        }
      } catch {
        /* Cross-origin styles do not own the launcher fonts. */
      }
    }
    fonts.textContent = faces.join('\n');
    native.head.append(fonts);
    return () => fonts.remove();
  }, [target, locale]);
  return target
    ? createPortal(
        <>
          <span
            className="spectator-score-player"
            title={names[0]}
          >{`P1${names[0] ? `(${names[0]})` : ''}`}</span>
          <span className="spectator-score-value">{`<${wins[0]} : ${wins[1]}>`}</span>
          <span
            className="spectator-score-player"
            title={names[1]}
          >{`P2${names[1] ? `(${names[1]})` : ''}`}</span>
        </>,
        target,
      )
    : null;
}
