import {useEffect, useLayoutEffect, useRef, useState, type ReactNode} from 'react';
import {useLocale} from '../../i18n';
import {AppLink} from './AppLink';
import {closeMainSelectMenus} from './MainSelect';

type Group = 'help' | 'settings';
export interface LauncherMastheadProps {
  interactive?: boolean;
  nativeLinks?: boolean;
  variant: 'library' | 'lobby' | 'help';
  assetUrl: (path: string) => string;
  launcherHref: string;
  faqHref: string;
  aboutHref: string;
  uptimeHref?: string;
  migrationHref?: string;
  migrationPolicyKnown?: boolean;
  updatedAge?: {text: string; title?: string; dateTime?: string};
  menuOpen: boolean;
  onMenuOpenChange: (open: boolean) => void;
  lessMotion: boolean;
  onToggleMotion: () => void;
  noticeEnabled: boolean;
  onToggleNotice: () => void;
  diagnosticsEnabled: boolean;
  onToggleDiagnostics: () => void;
  onDonation: () => void;
  donationAvailable?: boolean;
  onFirstUse: () => void;
  languageControl: ReactNode;
}

/** One navigation owner for the launcher, directory and standalone site pages. */
export function LauncherMasthead(props: LauncherMastheadProps) {
  const {locale, t} = useLocale();
  const {interactive = true, menuOpen} = props;
  const Link = interactive && !props.nativeLinks ? AppLink : 'a';
  const [group, setGroup] = useState<Group>('settings');
  const [compact, setCompact] = useState(false);
  const [sections, setSections] = useState<ReadonlySet<Group>>(() => new Set(['help', 'settings']));
  const root = useRef<HTMLElement>(null), inner = useRef<HTMLDivElement>(null);
  const toggle = useRef<HTMLButtonElement>(null), opener = useRef<HTMLElement | null>(null);
  const closeTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const openTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const callback = useRef(props.onMenuOpenChange); callback.current = props.onMenuOpenChange;
  const copy = (zh: string, en: string) => locale === 'en' ? en : zh;
  const base = new URL(props.launcherHref, 'https://launcher.invalid/');
  const href = (path: string) => {
    const result = new URL(path, base);
    return result.origin === 'https://launcher.invalid' && !props.launcherHref.startsWith('/')
      ? result.pathname.slice(1) + result.search + result.hash
      : result.origin === 'https://launcher.invalid' ? result.pathname + result.search + result.hash : result.href;
  };
  const labels = {single: copy('单机游玩', 'Games'), lobby: copy('联机大厅', 'Multiplayer'), help: copy('帮助', 'Help'), settings: copy('设置', 'Settings')};
  const cancelClose = () => {if (closeTimer.current !== null) clearTimeout(closeTimer.current); closeTimer.current = null;};
  const cancelOpen = () => {if (openTimer.current !== null) clearTimeout(openTimer.current); openTimer.current = null;};
  const close = () => {cancelOpen(); cancelClose(); callback.current(false); closeMainSelectMenus();};
  function open(next: Group, source?: HTMLElement, focus = false) {
    cancelOpen(); cancelClose(); setGroup(next); opener.current = source ?? toggle.current; callback.current(true);
    if (focus) requestAnimationFrame(() => root.current?.querySelector<HTMLElement>(`[data-site-panel="${next}"] .site-panel-content :is(a,button,select,[tabindex="0"])`)?.focus());
  }
  function hover(next: Group, source: HTMLElement) {
    cancelOpen(); cancelClose();
    openTimer.current = setTimeout(() => open(next, source), 500);
  }
  useEffect(() => {
    if (!interactive) return;
    const media = window.matchMedia?.('(max-width: 780px)');
    const changed = () => {setCompact(media?.matches ?? window.innerWidth <= 780); close();};
    changed(); media?.addEventListener('change', changed);
    const outside = (event: MouseEvent) => {
      if (event.target instanceof Element && event.target.closest('.mizuki-select-menu')) return;
      if (event.target instanceof Node && !root.current?.contains(event.target)) close();
    };
    document.addEventListener('click', outside);
    return () => {cancelOpen(); cancelClose(); media?.removeEventListener('change', changed); document.removeEventListener('click', outside);};
  }, [interactive]);
  useLayoutEffect(() => {
    if (!interactive) return;
    if (!menuOpen) closeMainSelectMenus();
    const measure = () => {
      root.current?.style.setProperty('--site-menu-height', `${inner.current?.getBoundingClientRect().height ?? 0}px`);
      if (!compact && root.current && inner.current) {
        const trigger = root.current.querySelector<HTMLElement>(`[data-site-trigger="${group}"]`);
        const panel = root.current.querySelector<HTMLElement>(`[data-site-panel="${group}"]`);
        if (trigger && panel) {
          const padding = parseFloat(getComputedStyle(inner.current).paddingLeft) || 0;
          const available = inner.current.clientWidth - padding * 2;
          const left = trigger.getBoundingClientRect().left - inner.current.getBoundingClientRect().left - padding;
          root.current.style.setProperty('--site-panel-offset', `${Math.max(0,Math.min(left,available-panel.getBoundingClientRect().width))}px`);
        }
      }
    };
    measure();
    const observer = typeof ResizeObserver === 'undefined' ? null : new ResizeObserver(measure);
    if (inner.current) observer?.observe(inner.current);
    return () => observer?.disconnect();
  }, [interactive, menuOpen, group, compact, sections, locale]);
  function section(next: Group, children: ReactNode) {
    return <section className="site-navigation-panel" data-site-panel={next} aria-label={labels[next]}>
      <button type="button" className="site-panel-heading masthead-link" aria-expanded={sections.has(next)} onClick={() => setSections(previous => {
        const updated = new Set(previous); if (updated.has(next)) updated.delete(next); else updated.add(next); return updated;
      })}>{labels[next]}<img className="site-caret" src={props.assetUrl('assets/room-caret-right.svg')} alt="" width="12" height="12"/></button>
      <div className="site-panel-content" hidden={compact && !sections.has(next)}>{children}</div>
    </section>;
  }
  return <header ref={root} id="mastheadMenu" className={`masthead site-header${props.variant === 'lobby' ? ' lobby-masthead' : ''}`}
    data-group={group} data-open={menuOpen} data-compact={compact} onPointerEnter={cancelClose}
    onPointerLeave={event => {cancelOpen(); if (!compact && event.pointerType === 'mouse') closeTimer.current = setTimeout(close, 160);}}
    onClick={event => {if ((event.target as Element).closest('a')) close();}}
    onKeyDown={event => {if (event.key === 'Escape') {cancelOpen(); if (menuOpen) {event.preventDefault(); event.stopPropagation(); close(); (opener.current ?? toggle.current)?.focus();}}}}>
    <div className="site-header-row">
      <Link className="brand" id="launcherLink" aria-label="EAGLER TOUHOU" href={props.launcherHref}>
        <span className="brand-eagler"><span className="brand-title">EAGLER</span>{props.updatedAge && <time className="brand-version" id="brandUpdateAge" title={props.updatedAge.title} dateTime={props.updatedAge.dateTime}>{props.updatedAge.text}</time>}</span>
        <span className="brand-separator" aria-hidden="true"><span className="brand-yinyang">☯</span></span><span className="brand-title">TOUHOU</span>
      </Link>
      <nav className="masthead-links site-primary-links" aria-label={t('nav.siteInfo')}>
        <Link className="masthead-link site-nav-page" data-site-link="single" aria-current={props.variant === 'library' ? 'page' : undefined} href={props.launcherHref} onPointerEnter={() => {if (!compact) close();}}>{labels.single}</Link>
        <Link className="masthead-link site-nav-page" data-site-link="lobby" aria-current={props.variant === 'lobby' ? 'page' : undefined} href={href('lobby.html')} onPointerEnter={() => {if (!compact) close();}}>{labels.lobby}</Link>
        {(['help', 'settings'] as const).map(next => <button key={next} type="button" className="masthead-link site-nav-group" data-site-trigger={next} aria-expanded={menuOpen && group === next} aria-controls="mastheadMenuPanel"
          onPointerEnter={event => {if (!compact && event.pointerType === 'mouse') hover(next, event.currentTarget);}} onPointerLeave={cancelOpen} onFocus={() => {cancelOpen(); cancelClose();}}
          onClick={event => {if (menuOpen && group === next) close(); else open(next, event.currentTarget);}}
          onKeyDown={event => {if (event.key === 'ArrowDown') {event.preventDefault(); open(next, event.currentTarget, true);}}}>{labels[next]}<img className="site-caret" src={props.assetUrl('assets/room-caret-right.svg')} alt="" width="12" height="12"/></button>)}
        <button className="masthead-link site-donation" id="donationOpenTop" type="button" hidden={props.donationAvailable === false} onClick={() => {close(); props.onDonation();}}>{t('nav.donate')}</button>
        <a className="masthead-link site-mobile-faq" href={props.faqHref}>{t('nav.faq')}</a>
        <button ref={toggle} className="masthead-link site-mobile-toggle" id="mastheadMenuToggle" type="button" aria-label={t('nav.more')} aria-expanded={menuOpen} aria-controls="mastheadMenuPanel"
          onClick={event => {opener.current = event.currentTarget; if (menuOpen) close(); else open('settings', event.currentTarget);}}>
          <img src={props.assetUrl(menuOpen ? 'assets/site-menu-close.svg' : 'assets/site-menu.svg')} alt="" width="22" height="22"/>
        </button>
      </nav>
    </div>
    <div className="site-navigation-tray" id="mastheadMenuPanel" inert={!menuOpen} aria-hidden={!menuOpen}>
      <div ref={inner} className="site-navigation-inner">
        <nav className="site-mobile-page-links" aria-label={copy('游玩入口', 'Play')}>
          <Link className="masthead-link site-page-entry" data-site-link="single" href={props.launcherHref}>{labels.single}</Link>
          <Link className="masthead-link site-page-entry" data-site-link="lobby" href={href('lobby.html')}>{labels.lobby}</Link>
        </nav>
        {section('help', <>
          <a className="masthead-menu-item" href={props.aboutHref}>{t('nav.about')}</a><a className="masthead-menu-item" href={props.faqHref}>{t('nav.faq')}</a>
          <a className="masthead-menu-item" href={props.uptimeHref ?? href('uptime/')}>{copy('服务器运行状态', 'Service status')}</a>
          <button className="masthead-menu-item" id="firstUseNoticeOpen" type="button" onClick={() => {close(); props.onFirstUse();}}>{t('nav.firstUseNotice')}</button>
          <a className="masthead-menu-item" id="originMigrationOpen" href={props.migrationHref ?? 'migrate.html'} hidden={!props.migrationHref} data-policy={props.migrationPolicyKnown ? 'host-manifest-origin-migration-policy/1' : undefined}>{t('nav.oldSiteMigration')}</a>
          <a className="masthead-menu-item" href="https://github.com/Ritosa2333/eagler-touhou" target="_blank" rel="noopener noreferrer">GitHub</a>
        </>)}
        {section('settings', <>
          <div className="site-language-row"><span>{copy('语言', 'Language')}</span><div className="ui-language-control">{props.languageControl}</div></div>
          <button className="masthead-menu-item site-setting" id="siteNoticeToggle" type="button" role="switch" aria-checked={props.noticeEnabled} onClick={props.onToggleNotice}><span>{t('notice.label')}</span><i className="site-setting-switch" aria-hidden="true"/></button>
          <button className="masthead-menu-item site-setting motion-menu-toggle" id={props.variant === 'lobby' ? 'lobbyLessMotion' : 'lessMotionToggle'} type="button" role="switch" aria-checked={props.lessMotion} onClick={props.onToggleMotion}><span>{t('nav.lessMotion')}</span><i className="site-setting-switch" aria-hidden="true"/></button>
          <button className="masthead-menu-item site-setting" id="runtimeDiagnosticsToggle" type="button" role="switch" aria-checked={props.diagnosticsEnabled} onClick={props.onToggleDiagnostics}><span>{t('diagnostics.toggle')}</span><i className="site-setting-switch" aria-hidden="true"/></button>
        </>)}
      </div>
    </div>
  </header>;
}
