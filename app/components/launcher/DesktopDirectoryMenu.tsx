import {useEffect, useLayoutEffect, useRef, useState, type RefObject} from 'react';
import {useLocale} from '../../i18n';
import {directoryCategoryKey, type DirectoryCategory, type DirectoryFilter} from './directory-categories';
import {DirectoryCategoryOptions} from './DirectoryCategoryOptions';
export {directoryCategoryKey} from './directory-categories';

/** The last group's first entry reaching the viewport bottom announces its arrival. */
export function enteredDirectoryCategory(rail: HTMLElement): DirectoryCategory | null {
  const bounds = rail.getBoundingClientRect(), bottom = Math.min(window.innerHeight, bounds.bottom);
  let entered: DirectoryCategory | null = null;
  for (const group of rail.querySelectorAll<HTMLElement>('[data-directory-group]:not([hidden])')) {
    const first = group.querySelector<HTMLElement>('.game:not([hidden]),.directory-coming-soon');
    if (first && first.getBoundingClientRect().top < bottom) entered = group.dataset.directoryGroup as DirectoryCategory;
  }
  return entered;
}

export function DesktopDirectoryMenu({directory, rail, category, onCategory, interactive, revision}: {
  directory: RefObject<HTMLElement | null>; rail: RefObject<HTMLDivElement | null>;
  category: DirectoryFilter; onCategory(category: DirectoryFilter): void; interactive: boolean; revision: string;
}) {
  const {t} = useLocale(), menu = useRef<HTMLDivElement>(null), options = useRef<HTMLDivElement>(null);
  const [expanded, setExpanded] = useState(true), [near, setNear] = useState(false), [focused, setFocused] = useState(false);
  const [cue, setCue] = useState<DirectoryCategory | null>(null);
  const open = expanded || near || focused || cue !== null, openRef = useRef(open); openRef.current = open;
  const cueTimer = useRef<ReturnType<typeof setTimeout> | undefined>(undefined);

  useLayoutEffect(() => {
    const node = directory.current;
    if (!node) return;
    const header = node.closest('.sheet')?.querySelector<HTMLElement>('.masthead') ?? document.querySelector<HTMLElement>('.masthead');
    const measure = () => {
      const bounds = node.getBoundingClientRect(), main = node.closest('.directory-layout');
      const gap = main ? Number.parseFloat(getComputedStyle(main).columnGap) || 28 : 28;
      const sheet = node.closest('.sheet');
      const spacing = sheet ? Number.parseFloat(getComputedStyle(sheet).rowGap) || 18 : 18;
      // Both the menu and workspace start one sheet gap below the Header.
      node.style.setProperty('--category-menu-top', `${(header?.getBoundingClientRect().height ?? 62) + spacing}px`);
      node.style.setProperty('--category-menu-width', `${Math.max(120, bounds.right + gap - 12)}px`);
      const width = Math.max(120,bounds.right + gap - 12), railBounds = rail.current?.getBoundingClientRect();
      const center = Math.max(52,Math.min(width - 52,railBounds ? railBounds.left + railBounds.width/2 : bounds.left + bounds.width/2));
      options.current?.style.setProperty('--category-align-x',`${center}px`);
      options.current?.style.setProperty('--category-scroll-end',`${Math.max(22,width - center - 44)}px`);
    };
    measure(); const resize = new ResizeObserver(measure); resize.observe(node);
    if (header) resize.observe(header);
    window.addEventListener('resize', measure);
    return () => {resize.disconnect(); window.removeEventListener('resize', measure); node.style.removeProperty('--category-menu-top'); node.style.removeProperty('--category-menu-width');};
  }, [directory,rail]);

  useEffect(() => {
    const scroller = rail.current, node = menu.current, choices = options.current;
    if (!scroller || !node || !choices || !interactive) return;
    let observed = enteredDirectoryCategory(scroller), userScroll = false;
    const clearSpace = () => {
      const frame = node.getBoundingClientRect(), railBounds = scroller.getBoundingClientRect();
      const top = Math.max(frame.top,railBounds.top,0), bottom = Math.min(frame.bottom,railBounds.bottom,window.innerHeight);
      return bottom <= top || ![...scroller.querySelectorAll<HTMLElement>('.game:not([hidden])')].some(card=>{
        const cover = card.getBoundingClientRect();
        return cover.bottom > top && cover.top < bottom && cover.right > 0 && cover.left < node.offsetWidth;
      });
    };
    clearTimeout(cueTimer.current); setCue(null); setExpanded(true);
    const intent = () => {userScroll = true;};
    const scroll = () => {
      const next = enteredDirectoryCategory(scroller);
      if (scroller.scrollTop <= 1) {
        clearTimeout(cueTimer.current);setCue(null);setExpanded(true);observed = next;return;
      }
      const empty = clearSpace();
      if (!userScroll) {if(empty)setExpanded(true);observed = next; return;}
      setExpanded(empty); setNear(false); setFocused(false);
      if (node.contains(document.activeElement) && document.activeElement instanceof HTMLElement) document.activeElement.blur();
      if (category === 'all' && next && next !== observed) {
        setCue(next); clearTimeout(cueTimer.current);
        cueTimer.current = setTimeout(() => setCue(null), 1600);
      }
      observed = next;
    };
    const proximity = (event: PointerEvent) => {
      if (event.pointerType !== 'mouse') return;
      const bounds = node.getBoundingClientRect();
      const nearby = event.clientX >= 0 && event.clientX <= (openRef.current ? bounds.width + 16 : 64) && event.clientY >= bounds.top - 18 && event.clientY <= bounds.bottom + 18;
      setNear(nearby);
      if (nearby) {clearTimeout(cueTimer.current); setCue(null);}
    };
    const keyIntent = (event: KeyboardEvent) => {if (scroller.contains(event.target as Node) && ['ArrowUp','ArrowDown','Home','End','PageUp','PageDown'].includes(event.key)) intent();};
    const wheel = (event: WheelEvent) => {
      if (event.ctrlKey || event.metaKey || choices.scrollWidth <= choices.clientWidth) return;
      event.preventDefault(); event.stopPropagation();
      const unit = event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? choices.clientWidth : 1;
      choices.scrollLeft += (Math.abs(event.deltaX) > Math.abs(event.deltaY) ? event.deltaX : event.deltaY) * unit;
    };
    scroller.addEventListener('scroll', scroll, {passive:true});
    const vacancy = new ResizeObserver(scroll);vacancy.observe(scroller);
    window.addEventListener('resize',scroll);
    scroller.addEventListener('wheel', intent, {passive:true}); scroller.addEventListener('pointerdown', intent, {passive:true}); scroller.addEventListener('pointermove', intent, {passive:true});
    node.addEventListener('wheel', wheel, {passive:false}); document.addEventListener('pointermove', proximity, {passive:true}); document.addEventListener('keydown', keyIntent);
    return () => {
      clearTimeout(cueTimer.current);vacancy.disconnect();window.removeEventListener('resize',scroll);scroller.removeEventListener('scroll', scroll); scroller.removeEventListener('wheel', intent); scroller.removeEventListener('pointerdown', intent); scroller.removeEventListener('pointermove', intent);
      node.removeEventListener('wheel', wheel); document.removeEventListener('pointermove', proximity); document.removeEventListener('keydown', keyIntent);
    };
  }, [rail, category, interactive, revision]);

  useEffect(() => {
    const node = menu.current, choices = options.current;
    if (!node || !choices || !interactive) return;
    const pointer = matchMedia('(min-width:781px) and (orientation:landscape) and (hover:hover) and (pointer:fine)');
    const reduced = matchMedia('(prefers-reduced-motion:reduce)');
    let frame = 0, x: number | null = null, last = 0, mouseInput = true;
    const reset = () => {cancelAnimationFrame(frame);frame = 0;x = null;last = 0;};
    const enabled = () => openRef.current && !choices.inert && pointer.matches && !reduced.matches && !document.body.classList.contains('less-motion') && !node.closest('[inert]') && !document.querySelector('dialog[open]');
    const update = (time: number) => {
      frame = 0;
      if (!enabled() || x === null) {reset();return;}
      const bounds = choices.getBoundingClientRect(), left = Math.max(0,bounds.left), right = Math.min(window.innerWidth,bounds.right);
      const edge = Math.min(44,Math.max(0,right - left)/3), elapsed = last ? Math.min(time - last,32) : 16;last = time;
      const speed = !edge || x < left || x > right ? 0 : x < left + edge ? -260*(1 - (x - left)/edge) : x > right - edge ? 260*(1 - (right - x)/edge) : 0;
      const before = choices.scrollLeft;
      choices.scrollLeft = Math.max(0,Math.min(choices.scrollWidth - choices.clientWidth,before + speed*elapsed/1000));
      if (speed && Math.abs(choices.scrollLeft - before) > .1) frame = requestAnimationFrame(update);
      else last = 0;
    };
    const move = (event: MouseEvent | PointerEvent) => {
      if ('pointerType' in event) mouseInput = event.pointerType === 'mouse';
      if (!mouseInput || !enabled() || choices.scrollWidth <= choices.clientWidth) {reset();return;}
      x = event.clientX;if (!frame) frame = requestAnimationFrame(update);
    };
    const start = (event: PointerEvent) => {mouseInput = event.pointerType === 'mouse';reset();};
    node.addEventListener('pointermove',move);node.addEventListener('mousemove',move);node.addEventListener('pointerleave',reset);node.addEventListener('mouseleave',reset);node.addEventListener('pointercancel',reset);node.addEventListener('pointerdown',start);
    pointer.addEventListener('change',reset);reduced.addEventListener('change',reset);window.addEventListener('blur',reset);document.addEventListener('visibilitychange',reset);
    return () => {
      reset();node.removeEventListener('pointermove',move);node.removeEventListener('mousemove',move);node.removeEventListener('pointerleave',reset);node.removeEventListener('mouseleave',reset);node.removeEventListener('pointercancel',reset);node.removeEventListener('pointerdown',start);
      pointer.removeEventListener('change',reset);reduced.removeEventListener('change',reset);window.removeEventListener('blur',reset);document.removeEventListener('visibilitychange',reset);
    };
  }, [category,interactive,revision]);

  const choose = (next: DirectoryFilter) => {if (!interactive) return; clearTimeout(cueTimer.current); setCue(null); setExpanded(true); onCategory(next);};
  return <div className="desktop-directory-menu" ref={menu} data-open={open} data-cue={cue !== null} aria-label={t('directory.categories')} tabIndex={interactive ? 0 : -1}
    onFocus={() => setFocused(true)} onBlur={event => {if (!event.currentTarget.contains(event.relatedTarget as Node | null)) setFocused(false);}}
    onKeyDown={event => {
      if (event.key === 'Escape') {event.preventDefault(); event.stopPropagation(); clearTimeout(cueTimer.current); setCue(null); setExpanded(false); setNear(false); event.currentTarget.focus({preventScroll:true}); setFocused(false);}
      else if (event.target === event.currentTarget && (event.key === 'Enter' || event.key === ' ')) {event.preventDefault(); setExpanded(true); options.current?.querySelector<HTMLButtonElement>('[aria-pressed="true"]')?.focus({preventScroll:true});}
    }}>
    <DirectoryCategoryOptions options={options} filter={category} onCategory={choose} interactive={interactive} open={open}/>
    <span className="directory-category-cue" role="status" aria-live="polite">{cue ? t(directoryCategoryKey(cue)) : ''}</span>
    <span className="directory-category-frame" aria-hidden="true"/>
  </div>;
}
