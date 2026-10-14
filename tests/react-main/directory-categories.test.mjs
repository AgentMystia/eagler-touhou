/** Mounted interaction fixtures; viewport geometry is synthetic, not visual acceptance. */
import test, {before, after, afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp, rm} from 'node:fs/promises';
import {resolve} from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';
import {build} from 'esbuild';
import {authoredSourcesPlugin} from './authored-sources.mjs';
import {installMountedDom} from './mounted-dom-environment.mjs';
const project=fileURLToPath(new URL('../../',import.meta.url));
let env, React, createRoot, api, work, root, originalRect, originalMedia, originalResize, height;
let desktop=true, media, resizeObservers;
before(async()=>{
 env=installMountedDom();React=await import('react');({createRoot}=await import('react-dom/client'));
 originalRect=env.window.HTMLElement.prototype.getBoundingClientRect;originalMedia=globalThis.matchMedia;originalResize=globalThis.ResizeObserver;height=Object.getOwnPropertyDescriptor(env.window,'innerHeight');
 work=await mkdtemp(resolve(project,'.cache/directory-category-tests-'));const output=resolve(work,'actual.mjs');
 await build({absWorkingDir:project,stdin:{resolveDir:project,loader:'tsx',contents:`
  export * from './app/components/launcher/GameDirectory.tsx';
  export * from './app/components/launcher/products.ts';
  export * from './app/components/launcher/directory-categories.ts';
  export * from './app/components/launcher/DesktopDirectoryMenu.tsx';
  export * from './app/i18n.tsx';
 `},outfile:output,bundle:true,platform:'node',format:'esm',jsx:'automatic',packages:'external',logLevel:'silent',plugins:[authoredSourcesPlugin(project)]});
 api=await import(pathToFileURL(output).href);
});
afterEach(async()=>{
 if(root){await React.act(async()=>root.unmount());root=null;}
 env.document.body.replaceChildren();env.window.HTMLElement.prototype.getBoundingClientRect=originalRect;
 globalThis.matchMedia=originalMedia;env.window.matchMedia=originalMedia;Object.defineProperty(env.window,'innerHeight',height);
 globalThis.ResizeObserver=env.window.ResizeObserver=originalResize;
 assert.deepEqual(env.errors.splice(0),[]);
});
after(async()=>{env.close();await rm(work,{recursive:true,force:true});});
function setup(narrow=false,motion=false){
 desktop=!narrow;media=new Map();
 resizeObservers=[];
 globalThis.ResizeObserver=env.window.ResizeObserver=class {constructor(callback){this.callback=callback;this.targets=new Set();resizeObservers.push(this);}observe(node){this.targets.add(node);}disconnect(){this.targets.clear();}};
 const matches=query=>query.includes('prefers-reduced-motion')&&!motion||query.includes('min-width:781px')&&desktop||query.includes('orientation:portrait')&&!desktop;
 globalThis.matchMedia=env.window.matchMedia=query=>{
  if(!media.has(query)){const listeners=new Set();media.set(query,{media:query,matches:matches(query),addEventListener:(type,fn)=>listeners.add(fn),removeEventListener:(type,fn)=>listeners.delete(fn),listeners});}
  return media.get(query);
 };
 Object.defineProperty(env.window,'innerHeight',{configurable:true,value:600});
 env.window.HTMLElement.prototype.getBoundingClientRect=function(){
  if(this.classList.contains('desktop-directory-menu'))return new env.window.DOMRect(0,110,220,60);
  if(this.classList.contains('game-rail'))return desktop ? new env.window.DOMRect(44,80,243,520) : new env.window.DOMRect(18,80,354,160);
  if(desktop && this.classList.contains('directory-site'))return new env.window.DOMRect(44,-100-this.closest('.game-rail').scrollTop,160,160);
  if(!desktop&&(this.classList.contains('game')||this.classList.contains('directory-coming-soon'))){
   const rail=this.closest('.game-rail'),index=api.visibleDirectoryEntries(rail).indexOf(this);
   return new env.window.DOMRect(18+111+index*154-rail.scrollLeft,94,132,132);
  }
  const group=this.closest('[data-directory-group]');
  if(group&&(this.classList.contains('game')||this.classList.contains('directory-coming-soon'))){
   const start={mainline:100,classic:700,decimal:1200,fan:2000}[group.dataset.directoryGroup];
   return new env.window.DOMRect(44,start-this.closest('.game-rail').scrollTop,160,160);
  }
  return originalRect.call(this);
 };
}
const products=()=>api.createLibraryProducts(['th08','th07','th06mp','th06'],path=>path,id=>'/?game='+id).map(p=>p.gameId==='th08'?{...p,directoryCategory:'fan'}:p.gameId==='th07'?{...p,number:'7.5'}:p);
const visible=selector=>[...env.document.querySelectorAll(selector)].filter(e=>!e.closest('[hidden]'));
const n=selector=>visible(selector)[0] ?? null;
const act=fn=>React.act(async()=>fn());
const click=value=>act(()=>n(`[data-category="${value}"]`).click());
async function mount(values=products(),extra={},narrow=false,motion=false){
 setup(narrow,motion);const target=env.document.createElement('main');env.document.body.append(target);root=createRoot(target);const activations=[];
 await act(()=>root.render(React.createElement(api.GameDirectory,{products:values,selectedProduct:'th06',onActivate:id=>activations.push(id),onSite:()=>activations.push('site'),siteHref:'/',...extra})));
 return {activations};
}
test('grouping honors explicit category metadata and deduplicates game modes without copying membership',()=>{
 const input=products(),copy=[...input],deduped=api.directoryProducts(input);
 assert.deepEqual(api.directoryGroups(deduped,'all').map(g=>[g.category,g.products.map(p=>p.id)]),[['mainline',['th06']],['classic',[]],['decimal',['th07']],['fan',['th08']]]);
 assert.deepEqual(api.directoryGroups(deduped,'decimal').map(g=>g.products.map(p=>p.id)),[['th07']]);
 assert.equal(api.directoryCategory({number:'06',directoryCategory:'fan'}),'fan');assert.equal(api.directoryCategory({number:'F1'}),'fan');assert.equal(api.directoryCategory({number:'01'}),'classic');assert.equal(api.directoryCategory({number:'05'}),'classic');assert.equal(api.directoryCategory({number:'06'}),'mainline');assert.deepEqual(input,copy);
});
test('desktop defaults to all, keeps the site entry and filters without activating a game',async()=>{
 const {activations}=await mount();
 assert.equal(n('[data-category="all"]').getAttribute('aria-pressed'),'true');assert.equal(n('.desktop-directory-menu').dataset.open,'true');
 assert.equal(visible('.directory-group').length,4);assert.equal(visible('.directory-group-divider').length,4);
 assert.equal(n('.directory-site-divider').parentElement.dataset.directoryGroup,'mainline');
 assert.deepEqual([...visible('.game[data-game]')].map(e=>e.dataset.game),['th06','th07','th08']);
 await click('decimal');assert.deepEqual([...visible('.game[data-game]')].map(e=>e.dataset.game),['th07']);
 assert.equal(n('.directory-group').dataset.directoryGroup,'decimal');assert.equal(n('.directory-group-divider'),null);assert.ok(n('.directory-site'));
 n('.game-rail').scrollTop=10000;await act(()=>n('.game-rail').dispatchEvent(new env.window.Event('scroll')));
 assert.equal(visible('.game[data-game]').length,1);assert.deepEqual(activations,[]);
 await click('all');assert.equal(visible('.game[data-game]').length,3);
});
test('empty categories show the requested four-character placeholder instead of other works',async()=>{
 await mount(api.createLibraryProducts(['th06'],path=>path,id=>'/?game='+id));
 assert.equal(visible('.directory-coming-soon').length,3);
 await click('fan');assert.equal(n('.directory-coming-soon').textContent,'敬请期待');assert.equal(visible('.game[data-game]').length,0);
 assert.equal(n('.directory-coming-soon').getAttribute('aria-label'),'同人作 · 敬请期待');assert.ok(n('.directory-site'));
});
test('a desktop category chosen from the site opens its first available game',async()=>{
 const {activations}=await mount(products(),{selectedProduct:undefined});await click('classic');assert.deepEqual(activations,[]);
 await click('mainline');assert.deepEqual(activations,['th06']);
});
test('narrow layout uses a persistent category menu and preserves the selected scope on resize',async()=>{
 await mount();await click('decimal');
 await act(()=>{desktop=false;for(const value of media.values()){value.matches=value.media.includes('prefers-reduced-motion')||value.media.includes('orientation:portrait');for(const fn of [...value.listeners])fn({matches:value.matches});}});
 assert.equal(n('.desktop-directory-menu'),null);assert.equal(n('.narrow-directory-menu').dataset.filter,'decimal');assert.equal(n('.directory-coming-soon'),null);
 assert.deepEqual([...visible('.game[data-game]')].map(e=>e.dataset.game),['th07']);assert.equal(n('.shelf-heading'),null);
 assert.equal(n('[data-category="all"]').textContent,'全部作品');
});
test('user scrolling retracts the menu and the first arriving pixel of a new group announces it temporarily',async context=>{
 await mount();context.mock.timers.enable({apis:['setTimeout']});const rail=n('.game-rail');
 await act(()=>{rail.scrollTop=100;rail.dispatchEvent(new env.window.Event('scroll'));});assert.equal(n('.desktop-directory-menu').dataset.open,'true','programmatic initial alignment does not retract the menu');
 await act(()=>{rail.dispatchEvent(new env.window.WheelEvent('wheel',{bubbles:true,deltaY:20}));rail.scrollTop=70;rail.dispatchEvent(new env.window.Event('scroll'));});
 assert.equal(n('.desktop-directory-menu').dataset.open,'false');assert.equal(n('.directory-category-cue').textContent,'');
 await act(()=>{rail.scrollTop=600.5;rail.dispatchEvent(new env.window.Event('scroll'));});
 assert.equal(n('.directory-category-cue').textContent,'小数点作');assert.equal(n('.desktop-directory-menu').dataset.open,'true');assert.equal(n('[data-category="all"]').getAttribute('aria-pressed'),'true','an arrival notice does not change filtering');
 await act(()=>context.mock.timers.tick(1600));assert.equal(n('.desktop-directory-menu').dataset.open,'true','the empty menu area remains available after the arrival notice');
 await act(()=>{rail.scrollTop=1400.5;rail.dispatchEvent(new env.window.Event('scroll'));});assert.equal(n('.directory-category-cue').textContent,'同人作');
 await act(()=>context.mock.timers.tick(1600));
 await act(()=>{rail.scrollTop=1890;rail.dispatchEvent(new env.window.Event('scroll'));});assert.equal(n('.desktop-directory-menu').dataset.open,'false','a cover occupying the menu area retracts it again');
 await act(()=>env.document.dispatchEvent(new env.window.PointerEvent('pointermove',{pointerType:'mouse',clientX:30,clientY:130})));
 assert.equal(n('.directory-category-cue').textContent,'');assert.equal(n('.desktop-directory-menu').dataset.open,'true');
 await act(()=>env.document.dispatchEvent(new env.window.PointerEvent('pointermove',{pointerType:'mouse',clientX:600,clientY:300})));
 assert.equal(n('.desktop-directory-menu').dataset.open,'false');
});
test('desktop menu opens at the top and in free space, but retracts beneath a cover',async()=>{
 await mount();const rail=n('.game-rail');
 await act(()=>{rail.dispatchEvent(new env.window.WheelEvent('wheel',{bubbles:true,deltaY:20}));rail.scrollTop=70;rail.dispatchEvent(new env.window.Event('scroll'));});assert.equal(n('.desktop-directory-menu').dataset.open,'false');
 await act(()=>{rail.scrollTop=0;rail.dispatchEvent(new env.window.Event('scroll'));});assert.equal(n('.desktop-directory-menu').dataset.open,'true');
 await act(()=>{rail.scrollTop=100;rail.dispatchEvent(new env.window.Event('scroll'));});assert.equal(n('.desktop-directory-menu').dataset.open,'false');
 await act(()=>{rail.scrollTop=190;rail.dispatchEvent(new env.window.Event('scroll'));});assert.equal(n('.desktop-directory-menu').dataset.open,'true','no cover occupies the menu area');
});
test('menu wheel scrolls its options horizontally without scrolling games or selecting categories',async()=>{
 const {activations}=await mount(),choices=n('.directory-category-options'),rail=n('.game-rail');
 Object.defineProperties(choices,{clientWidth:{configurable:true,value:120},scrollWidth:{configurable:true,value:300}});
 const event=new env.window.WheelEvent('wheel',{bubbles:true,cancelable:true,deltaY:25});await act(()=>choices.dispatchEvent(event));
 assert.equal(event.defaultPrevented,true);assert.equal(choices.scrollLeft,25);assert.equal(rail.scrollTop,0);assert.deepEqual(activations,[]);assert.equal(n('[data-category="all"]').getAttribute('aria-pressed'),'true');
});
test('scrolling works retracts a focused filter and the enlarged menu remains keyboard-accessible without a head button',async()=>{
 const {activations}=await mount();await act(()=>n('[data-category="mainline"]').focus());await click('mainline');
 assert.equal(n('.directory-category-head'),null);assert.equal(n('.directory-site-divider').textContent,'正作');
 const rail=n('.game-rail');await act(()=>{rail.dispatchEvent(new env.window.WheelEvent('wheel',{bubbles:true,deltaY:20}));rail.scrollTop=100;rail.dispatchEvent(new env.window.Event('scroll'));});
 assert.equal(n('.desktop-directory-menu').dataset.open,'false');assert.equal(env.document.activeElement,env.document.body);
 const menu=n('.desktop-directory-menu');await act(()=>menu.focus());assert.equal(menu.dataset.open,'true');
 await act(()=>menu.dispatchEvent(new env.window.KeyboardEvent('keydown',{key:'Enter',bubbles:true,cancelable:true})));
 assert.equal(env.document.activeElement,n('[data-category="mainline"]'));
 await act(()=>n('[data-category="mainline"]').dispatchEvent(new env.window.KeyboardEvent('keydown',{key:'Escape',bubbles:true,cancelable:true})));
 assert.equal(menu.dataset.open,'false');assert.equal(env.document.activeElement,menu);
 await act(()=>menu.dispatchEvent(new env.window.KeyboardEvent('keydown',{key:'ArrowDown',bubbles:true,cancelable:true})));
 assert.deepEqual(activations,[],'menu navigation does not activate another game');
});
test('locked rooms may browse a category while game activation remains disabled',async()=>{
 const {activations}=await mount(products(),{locked:true});await click('decimal');
 const card=n('.game[data-game="th07"]');assert.equal(card.getAttribute('href'),null);assert.equal(card.getAttribute('aria-disabled'),'true');
 await act(()=>card.click());assert.deepEqual(activations,[]);
});
const scrollCenter=left=>act(()=>{const rail=n('.game-rail');rail.dispatchEvent(new env.window.WheelEvent('wheel',{bubbles:true,deltaX:20}));rail.scrollLeft=left;rail.dispatchEvent(new env.window.Event('scroll'));rail.dispatchEvent(new env.window.Event('scrollend'));});
test('narrow all-work browsing follows the centered category without turning it into a filter',async()=>{
 const {activations}=await mount(products(),{},true);
 assert.equal(n('.game-rail').scrollLeft,154);assert.equal(n('.narrow-directory-menu').dataset.highlight,'mainline');
 await scrollCenter(462);assert.equal(n('.narrow-directory-menu').dataset.highlight,'decimal');assert.equal(n('.narrow-directory-menu').dataset.filter,'all');
 assert.equal(n('[data-category="all"]').getAttribute('aria-pressed'),'true');assert.equal(n('[data-category="decimal"]').getAttribute('data-active'),'true');
 await scrollCenter(616);assert.equal(n('.narrow-directory-menu').dataset.highlight,'fan');assert.equal(visible('.game[data-game]').length,3);
 assert.deepEqual(activations,['th07','th08']);
 await click('decimal');assert.equal(n('.narrow-directory-menu').dataset.filter,'decimal');assert.deepEqual([...visible('.game[data-game]')].map(e=>e.dataset.game),['th07']);
 const count=activations.length;await scrollCenter(616);assert.ok(activations.slice(count).every(id=>id==='th07'));assert.equal(n('.narrow-directory-menu').dataset.highlight,'decimal');
});
test('classic follows mainline and filters to its own empty placeholder without game navigation',async()=>{
 const {activations}=await mount(products(),{},true);
 assert.deepEqual([...env.document.querySelectorAll('.narrow-directory-menu [data-category]')].map(e=>e.dataset.category),['all','mainline','classic','decimal','fan']);
 await scrollCenter(308);assert.equal(n('.narrow-directory-menu').dataset.highlight,'classic');assert.equal(n('.narrow-directory-menu').dataset.filter,'all');assert.deepEqual(activations,[]);
 await click('classic');assert.equal(n('.directory-group').dataset.directoryGroup,'classic');assert.equal(n('.directory-coming-soon').textContent,'敬请期待');assert.equal(visible('.game[data-game]').length,0);assert.deepEqual(activations,[]);
});
test('narrow category selection from the site commits the centered game after settlement and survives resize',async context=>{
 const {activations}=await mount(products(),{selectedProduct:undefined},true);context.mock.timers.enable({apis:['setTimeout']});
 assert.equal(n('.game-rail').scrollLeft,0);await click('mainline');assert.equal(n('.game-rail').scrollLeft,154);assert.deepEqual(activations,[]);
 const resize=resizeObservers.find(observer=>observer.targets.has(n('.game-rail')));await act(()=>resize.callback());
 await act(()=>context.mock.timers.tick(180));assert.deepEqual(activations,['th06']);
 await act(()=>n('.game-rail').dispatchEvent(new env.window.Event('scrollend')));assert.deepEqual(activations,['th06']);
 await click('classic');await act(()=>context.mock.timers.tick(180));assert.deepEqual(activations,['th06'],'an empty category cannot invent a game');
 await click('decimal');await act(()=>context.mock.timers.tick(180));assert.deepEqual(activations,['th06','th07']);
});
test('a scroll interruption cancels pending category activation and chooses the actual settled entry',async context=>{
 const {activations}=await mount(products(),{selectedProduct:undefined},true);context.mock.timers.enable({apis:['setTimeout']});
 await click('mainline');await scrollCenter(0);await act(()=>context.mock.timers.tick(180));assert.deepEqual(activations,[],'returning the selector to the already-selected site cancels the pending game');
});
test('desktop category edges scroll only menu choices and stop for leave, touch or reduced motion',async context=>{
 const frames=new Map();let next=1;
 context.mock.method(globalThis,'requestAnimationFrame',callback=>{const id=next++;frames.set(id,callback);return id;});
 context.mock.method(globalThis,'cancelAnimationFrame',id=>frames.delete(id));
 const {activations}=await mount(products(),{},false,true),menu=n('.desktop-directory-menu'),choices=n('.directory-category-options');
 choices.getBoundingClientRect=()=>new env.window.DOMRect(0,110,220,60);
 Object.defineProperties(choices,{clientWidth:{configurable:true,value:220},scrollWidth:{configurable:true,value:470}});
 const move=(x,type='mouse')=>act(()=>menu.dispatchEvent(new env.window.PointerEvent('pointermove',{bubbles:true,pointerType:type,clientX:x,clientY:130})));
 const step=time=>act(()=>{const pending=[...frames.values()];frames.clear();pending.forEach(callback=>callback(time));});
 await move(215);await step(16);assert.ok(choices.scrollLeft>0);const right=choices.scrollLeft;
 await move(5);await step(32);assert.ok(choices.scrollLeft<right);assert.equal(n('.game-rail').scrollTop,0);assert.deepEqual(activations,[]);
 await move(215);await act(()=>menu.dispatchEvent(new env.window.PointerEvent('pointerleave')));assert.equal(frames.size,0);
 await act(()=>menu.dispatchEvent(new env.window.MouseEvent('mousemove',{bubbles:true,clientX:215,clientY:130})));assert.ok(frames.size>0,'legacy mouse movement can start edge scrolling');
 await act(()=>menu.dispatchEvent(new env.window.PointerEvent('pointerdown',{pointerType:'touch'})));assert.equal(frames.size,0);
 await act(()=>menu.dispatchEvent(new env.window.MouseEvent('mousemove',{bubbles:true,clientX:215,clientY:130})));assert.equal(frames.size,0,'touch compatibility mouse events cannot start edge scrolling');
 await move(215,'touch');assert.equal(frames.size,0);
 await move(215);const reduced=media.get('(prefers-reduced-motion:reduce)');await act(()=>{reduced.matches=true;for(const fn of reduced.listeners)fn({matches:true});});assert.equal(frames.size,0);
 await move(215);assert.equal(frames.size,0);assert.deepEqual(activations,[]);
});
test('narrow All is actionable when its filter is already All and commits the site after recentering',async context=>{
 const {activations}=await mount(products(),{},true);context.mock.timers.enable({apis:['setTimeout']});
 await scrollCenter(462);assert.equal(n('.narrow-directory-menu').dataset.highlight,'decimal');assert.equal(n('.narrow-directory-menu').dataset.filter,'all');
 await click('all');assert.equal(n('.game-rail').scrollLeft,0);assert.equal(n('.narrow-directory-menu').dataset.highlight,'all');
 assert.deepEqual(activations,['th07'],'no site navigation before the scroll settles');
 await act(()=>context.mock.timers.tick(180));assert.deepEqual(activations,['th07','site']);
 await act(()=>n('.game-rail').dispatchEvent(new env.window.Event('scrollend')));assert.deepEqual(activations,['th07','site'],'late scrollend does not duplicate the settled site callback');
});
test('narrow All rebuilds every group and preserves the site return through resize',async context=>{
 const {activations}=await mount(products(),{},true);context.mock.timers.enable({apis:['setTimeout']});
 await click('fan');assert.equal(n('.narrow-directory-menu').dataset.filter,'fan');
 await click('all');assert.equal(n('.game-rail').scrollLeft,0);assert.equal(visible('.directory-group').length,4);
 const resize=resizeObservers.find(observer=>observer.targets.has(n('.game-rail')));
 await act(()=>resize.callback());assert.equal(n('.game-rail').scrollLeft,0);
 await act(()=>context.mock.timers.tick(180));assert.deepEqual(activations,['site']);
});
test('a user scroll may interrupt an All return without a late site activation',async context=>{
 const {activations}=await mount(products(),{},true);context.mock.timers.enable({apis:['setTimeout']});
 await click('all');await scrollCenter(616);await act(()=>context.mock.timers.tick(180));
 assert.deepEqual(activations,['th08']);assert.equal(n('.narrow-directory-menu').dataset.highlight,'fan');
});
test('All returns a locked or disabled-site rail without bypassing activation guards',async context=>{
 const {activations}=await mount(products(),{siteDisabled:true},true);context.mock.timers.enable({apis:['setTimeout']});
 await click('all');assert.equal(n('.game-rail').scrollLeft,0);
 await act(()=>n('.game-rail').dispatchEvent(new env.window.Event('scrollend')));await act(()=>context.mock.timers.tick(180));assert.deepEqual(activations,[]);
 await act(()=>root.unmount());root=null;env.document.body.replaceChildren();
 const locked=await mount(products(),{locked:true},true);await scrollCenter(616);await click('all');
 assert.equal(n('.game-rail').scrollLeft,0);assert.equal(n('.directory-rail-shell').dataset.centerSelect,'false');
 await act(()=>context.mock.timers.tick(180));assert.deepEqual(locked.activations,[]);
});
test('desktop All may return an already unfiltered rail to its top without activating a game',async()=>{
 const {activations}=await mount();n('.game-rail').scrollTop=450;
 await click('all');assert.equal(n('.game-rail').scrollTop,0);assert.equal(n('.desktop-directory-menu [aria-pressed=true]').dataset.category,'all');assert.deepEqual(activations,[]);
});
test('narrow empty categories can occupy the center without creating game navigation',async()=>{
 const {activations}=await mount(api.createLibraryProducts(['th06'],path=>path,id=>'/?game='+id),{},true);
 await scrollCenter(462);assert.equal(n('.narrow-directory-menu').dataset.highlight,'decimal');
 await scrollCenter(616);assert.equal(n('.narrow-directory-menu').dataset.highlight,'fan');assert.equal(n('.narrow-directory-menu').dataset.filter,'all');assert.deepEqual(activations,[]);
 const resize=resizeObservers.find(observer=>observer.targets.has(n('.game-rail')));
 await act(()=>resize.callback());assert.equal(n('.game-rail').scrollLeft,616,'resize retains the browsed empty category instead of jumping to the current game');
 await act(()=>resize.callback());assert.equal(n('.game-rail').scrollLeft,616,'a repeated resize retains the same alignment target');
 await click('fan');assert.equal(n('.game-rail').scrollLeft,154);assert.equal(n('.directory-coming-soon').textContent,'敬请期待');assert.equal(visible('.game[data-game]').length,0);assert.deepEqual(activations,[]);
});
test('narrow lock keeps the free rail and current category while manual filtering cannot change a game',async()=>{
 const {activations}=await mount(products(),{locked:true},true);assert.equal(n('.directory-rail-shell').dataset.centerSelect,'false');
 assert.equal(n('.narrow-directory-menu').dataset.highlight,'mainline');await scrollCenter(616);assert.deepEqual(activations,[]);
 await click('fan');assert.equal(n('.narrow-directory-menu').dataset.highlight,'fan');await act(()=>n('.game[data-game="th08"]').click());assert.deepEqual(activations,[]);
});

test('filtering retains loaded image owners while hidden works cannot take carousel selection',async()=>{
 const {activations}=await mount(products(),{},true),image=n('.game[data-game="th06"] img'),fan=n('.game[data-game="th08"] img');
 await click('fan');assert.equal(image.closest('.directory-group').hidden,true);assert.equal(env.document.querySelector('.game[data-game="th08"] img'),fan);
 await click('all');assert.equal(env.document.querySelector('.game[data-game="th06"] img'),image);assert.equal(env.document.querySelector('.game[data-game="th08"] img'),fan);
 assert.deepEqual(activations,[]);
});
test('desktop rear category choices center on the game rail instead of staying clipped at the right edge',async()=>{
 await mount();const choices=n('.directory-category-options'),active=n('[data-category="fan"]');
 Object.defineProperties(choices,{clientWidth:{configurable:true,value:220},scrollWidth:{configurable:true,value:600}});
 Object.defineProperties(active,{offsetLeft:{configurable:true,value:388},offsetWidth:{configurable:true,value:88}});
 choices.style.setProperty('--category-align-x','165.5');await click('fan');assert.equal(choices.scrollLeft,266.5);
 assert.equal(active.offsetLeft + active.offsetWidth/2 - choices.scrollLeft,165.5);
 choices.scrollLeft=100;await click('fan');assert.equal(choices.scrollLeft,266.5,'choosing the active option again also restores its alignment');
});

test('programmatic alignment and late scrollend events never replace the selected route',async context=>{
 const {activations}=await mount(products(),{selectedProduct:'th07'},true);context.mock.timers.enable({apis:['setTimeout']});
 const rail=n('.game-rail');await act(()=>rail.dispatchEvent(new env.window.Event('scrollend')));
 await act(()=>{rail.scrollLeft=0;rail.dispatchEvent(new env.window.Event('scroll'));rail.dispatchEvent(new env.window.Event('scrollend'));});await act(()=>context.mock.timers.tick(180));assert.deepEqual(activations,[]);
 await scrollCenter(0);assert.deepEqual(activations,['site'],'an actual wheel gesture still commits the site');
});
