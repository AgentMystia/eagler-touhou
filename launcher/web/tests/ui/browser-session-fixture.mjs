import { authoredSourcesPlugin } from './authored-sources.mjs';
/** Actual BrowserLauncher -> BrowserSession -> canonical owners under synthetic
 * DOM/Memory Router (opt-in jsdom BrowserRouter)/native-message/network/storage edges. No browser, engine,
 * rendering, real transport or durable IndexedDB acceptance is claimed.
 * Main authorities: app.mts4114–4289 (Start order), 3033–3063 (input),
 * 6220–6440/6850–6889 (files), 8531/8638 (same-product settings lifetime),
 * runtime-session and launcher-lifecycle. All original main tests are retained.
 */
import test, { before, after, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { runInNewContext } from 'node:vm';
import { mkdir, mkdtemp, readFile, rm } from 'node:fs/promises';
import { resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { build, transformSync } from 'esbuild';
import { IDBFactory, IDBKeyRange } from 'fake-indexeddb';
import { Blob as NativeBlob, File as NativeFile } from 'node:buffer';
import { createHash } from 'node:crypto';
import { zipSync, strToU8 } from 'fflate';
import { installMountedDom } from './mounted-dom-environment.mjs';
const project = fileURLToPath(new URL('../../', import.meta.url));
let env,
  work,
  api,
  React,
  createRoot,
  hydrateRoot,
  renderToString,
  createMemoryRouter,
  createBrowserRouter,
  RouterProvider,
  mounted;
const originals = new Map(),
  nativeWindows = new WeakMap();
const originalWarn = console.warn;
function expose(key, value) {
  if (!originals.has(key)) originals.set(key, Object.getOwnPropertyDescriptor(globalThis, key));
  Object.defineProperty(globalThis, key, {
    configurable: true,
    writable: true,
    value,
  });
}
before(async () => {
  env = installMountedDom();
  console.warn = (...args) => {
    mounted?.warnings.push({
      message: args.map(String).join(' '),
      stack: new Error().stack,
    });
    originalWarn(...args);
  };
  expose('location', env.window.location);
  expose('screen', env.window.screen);
  expose('Blob', NativeBlob);
  expose('File', NativeFile);
  Object.defineProperty(env.window.navigator, 'maxTouchPoints', {
    configurable: true,
    value: 0,
  });
  env.window.HTMLImageElement.prototype.decode = async function () {};
  env.window.HTMLElement.prototype.scrollIntoView = function () {};
  env.window.scrollTo = (options) => mounted?.scrolls.push(options);
  const anchorClick = env.window.HTMLAnchorElement.prototype.click;
  env.window.HTMLAnchorElement.prototype.click = function () {
    if (this.download) {
      mounted.downloads.push({
        name: this.download,
        url: this.href,
      });
      return;
    }
    anchorClick.call(this);
  };
  env.window.HTMLElement.prototype.requestFullscreen = async function () {
    mounted.order.push('fullscreen-enter');
    mounted.fullscreen = this;
    env.document.dispatchEvent(new env.window.Event('fullscreenchange'));
    if (mounted.fullscreenReady) await mounted.fullscreenReady;
  };
  Object.defineProperty(env.document, 'fullscreenElement', {
    configurable: true,
    get: () => mounted?.fullscreen ?? null,
  });
  env.document.exitFullscreen = async () => {
    mounted.order.push('fullscreen-exit');
    mounted.fullscreen = null;
    env.document.dispatchEvent(new env.window.Event('fullscreenchange'));
  };
  Object.defineProperty(env.window.HTMLIFrameElement.prototype, 'contentWindow', {
    configurable: true,
    get() {
      if (!nativeWindows.has(this)) nativeWindows.set(this, createNativeEndpoint(this));
      return nativeWindows.get(this);
    },
  });
  React = await import('react');
  ({ createRoot, hydrateRoot } = await import('react-dom/client'));
  ({ renderToString } = await import('react-dom/server'));
  ({ createMemoryRouter, createBrowserRouter, RouterProvider } = await import('react-router'));
  await mkdir(resolve(project, '.cache'), {
    recursive: true,
  });
  work = await mkdtemp(resolve(project, '.cache/ui-browser-session-'));
  const outfile = resolve(work, 'actual-browser-entry.mjs');
  await build({
    absWorkingDir: project,
    stdin: {
      resolveDir: project,
      loader: 'ts',
      contents: `
    export {BrowserLauncher} from './app/BrowserLauncher.tsx';
    export {getTestSession} from 'captured-browser-session';
    export {getAcquisitionOptions} from 'captured-acquisition';
    export {setPackageReadGate} from 'gated-package-read';
    export {validateSiteManifest} from '@eagler/contracts/site-manifest';
    export {PRODUCT_GAMES, multiplayerConfigForProduct} from '@eagler/contracts/product-catalog';
    export {encodeRoomInvite} from './src/launcher/room-invite.mts';
    export {validateReleaseCatalog} from '@eagler/contracts/release-catalog';
    export {readCurrentPackageGeneration} from './package/package-store.mjs';
    export {parsePackageZip} from './package/package-zip.mjs';
    export {installParsedPackageZip} from './package/package-installer.mjs';
    export {resolveRoomInvite} from './src/launcher/route-state.mts';
    export {gamePreferenceStorageKey, languagePreferenceStorageKey, sharedTouchPreferenceStorageKey} from './src/launcher/game-preferences.mts';
    export {touchLayoutStorageKey} from './src/launcher/touch-layout-model.mts';
    export {RUNTIME_EPOCH_QUERY_PARAMETER} from '@eagler/contracts/runtime-protocol';
  `,
    },
    outfile,
    bundle: true,
    platform: 'node',
    format: 'esm',
    jsx: 'automatic',
    packages: 'external',
    logLevel: 'silent',
    plugins: [
      {
        name: 'native-acquisition-scheduling',
        setup(ctx) {
          // Test-only references and a storage-read scheduling edge. Both wrappers
          // delegate to the authored owners; no preparation/queue policy is replaced.
          ctx.onResolve(
            {
              filter: /^(captured-browser-session|gated-package-read|captured-acquisition)$/,
            },
            (args) => ({
              path: args.path,
              namespace: 'session-edge',
            }),
          );
          ctx.onResolve(
            {
              filter: /session\/browser-session$/,
            },
            (args) =>
              args.importer.replaceAll('\\', '/').endsWith('/app/BrowserLauncher.tsx')
                ? {
                    path: 'captured-browser-session',
                    namespace: 'session-edge',
                  }
                : undefined,
          );
          ctx.onResolve(
            {
              filter: /package-store\.mjs$/,
            },
            (args) =>
              args.importer.replaceAll('\\', '/').endsWith('/app/services/package-acquisition.ts')
                ? {
                    path: 'gated-package-read',
                    namespace: 'session-edge',
                  }
                : undefined,
          );
          ctx.onResolve(
            {
              filter: /services\/package-acquisition$/,
            },
            (args) =>
              args.importer.replaceAll('\\', '/').endsWith('/app/session/browser-session.ts')
                ? {
                    path: 'captured-acquisition',
                    namespace: 'session-edge',
                  }
                : undefined,
          );
          ctx.onLoad(
            {
              filter: /.*/,
              namespace: 'session-edge',
            },
            (args) => ({
              loader: 'ts',
              resolveDir: project,
              contents:
                args.path === 'captured-browser-session'
                  ? `
        import {createBrowserSession as create} from './app/session/browser-session.ts';
        let session; export const getTestSession = () => session;
        export function createBrowserSession(options) {return session = create(options);}
      `
                  : args.path === 'captured-acquisition'
                    ? `
        import {createPackageAcquisition as create} from './app/services/package-acquisition.ts';
        let options; export const getAcquisitionOptions = () => options;
        export function createPackageAcquisition(value) {options = value; return create(value);}
      `
                    : `
        import {readCurrentPackageGeneration as readCurrent} from './package/package-store.mjs';
        let gate; export function setPackageReadGate(value) {gate = value;}
        export function readCurrentPackageGeneration(game) {const wait = gate?.(game); return wait ? Promise.resolve(wait).then(() => readCurrent(game)) : readCurrent(game);}
      `,
            }),
          );
        },
      },
      authoredSourcesPlugin(project),
    ],
  });
  api = await import(pathToFileURL(outfile).href);
});
afterEach(async () => {
  api?.setPackageReadGate(null);
  if (mounted?.root) {
    const current = mounted;
    await React.act(async () => {
      current.root.unmount();
      current.releaseHost?.();
      current.releaseFirstUse?.();
      await Promise.resolve();
    });
    current.router.dispose();
    assert.equal(env.document.querySelectorAll('#gameFrame').length, 0);
    assert.equal(
      env.window.__eaglerPrepareManagedRuntimeDataV1,
      undefined,
      'unmounted actual session releases its sole managed DATA host callback',
    );
    assert.deepEqual(env.errors, [], 'actual BrowserLauncher emits no uncaught DOM/React errors');
  }
  mounted = null;
  env.document.body.replaceChildren();
});
after(async () => {
  console.warn = originalWarn;
  for (const [key, descriptor] of originals) {
    if (descriptor) Object.defineProperty(globalThis, key, descriptor);
    else delete globalThis[key];
  }
  env?.close();
  if (work)
    await rm(work, {
      recursive: true,
      force: true,
    });
});
class MemoryStorage {
  values = new Map();
  writes = [];
  removals = [];
  blocked = false;
  getItem(key) {
    return this.values.get(key) ?? null;
  }
  setItem(key, value) {
    this.writes.push([key, String(value)]);
    if (this.blocked) throw new Error('Synthetic blocked storage');
    this.values.set(key, String(value));
  }
  removeItem(key) {
    this.removals.push(key);
    if (this.blocked) throw new Error('Synthetic blocked storage');
    this.values.delete(key);
  }
}
/** Native transport edge only. The actual room owner parses/normalizes every
 * message and owns membership, preparation, persistence and leave semantics. */
function installSocketBoundary(enabled, directoryTransport = false) {
  class Socket extends env.window.EventTarget {
    readyState = 0;
    sent = [];
    closes = [];
    constructor(url) {
      super();
      this.url = String(url);
      assert.equal(enabled, true, 'Unexpected WebSocket construction in this fixture');
      const target = new URL(this.url);
      assert.equal(target.hostname, 'relay.invalid');
      assert.ok(
        target.searchParams.get('lobby') ||
          (directoryTransport && target.searchParams.get('directory') === '1'),
        'only the opted-in actual room/directory transport is accepted',
      );
      mounted.sockets.push(this);
    }
    open() {
      this.readyState = 1;
      this.dispatchEvent(new env.window.Event('open'));
    }
    message(value) {
      this.dispatchEvent(
        new env.window.MessageEvent('message', {
          data: JSON.stringify(value),
        }),
      );
    }
    send(value) {
      assert.equal(this.readyState, 1);
      this.sent.push(JSON.parse(value));
    }
    close(code = 1000, reason = '') {
      this.closes.push({
        code,
        reason,
      });
      this.readyState = 3;
      this.dispatchEvent(
        new env.window.CloseEvent('close', {
          code,
          reason,
        }),
      );
    }
  }
  env.window.WebSocket = Socket;
  expose('WebSocket', Socket);
}
function createNativeEndpoint(frame) {
  const owner = mounted,
    native = new env.window.EventTarget();
  native.HTMLIFrameElement = env.window.HTMLIFrameElement;
  native.document = env.document.implementation.createHTMLDocument('Synthetic native Runtime');
  native.focus = () => owner.order.push('native-focus');
  native.Module = {};
  native.FS = {
    mkdirTree() {},
    writeFile(path, bytes) {
      owner.nativeWrites.push({
        path,
        bytes: [...bytes],
      });
    },
  };
  native.location = {
    href: 'about:blank',
    replace(url) {
      owner.order.push(url === 'about:blank' ? 'native-retired' : 'native-navigation');
      owner.navigations.push(url);
      this.href = url;
      native.document = env.document.implementation.createHTMLDocument('Synthetic native Runtime');
      if (url !== 'about:blank') {
        const parsed = new URL(url);
        native.envelope = {
          protocol: 'eagler-touhou/1',
          game: parsed.pathname.match(/th\d{2}/)?.[0],
          epoch: Number(parsed.searchParams.get(api.RUNTIME_EPOCH_QUERY_PARAMETER)),
        };
        if (owner.autoReady)
          queueMicrotask(async () => {
            if (parsed.searchParams.get('managedData') === '1') {
              const data = await env.window.__eaglerPrepareManagedRuntimeDataV1({
                game: native.envelope.game,
                generation: parsed.searchParams.get('gameGeneration'),
                epoch: native.envelope.epoch,
              });
              owner.dataReads.push([...new Uint8Array(data.buffer)]);
            }
            native.emit('ready');
          });
      }
    },
  };
  const setAttribute = frame.setAttribute.bind(frame),
    removeAttribute = frame.removeAttribute.bind(frame);
  frame.setAttribute = (name, value) => {
    if (name === 'src') native.location.replace(value);
    else setAttribute(name, value);
  };
  frame.removeAttribute = (name) => {
    if (name === 'src') native.location.replace('about:blank');
    else removeAttribute(name);
  };
  native.emit = (event, extra = {}) => {
    const message = new env.window.Event('message');
    Object.defineProperties(message, {
      origin: {
        value: env.window.location.origin,
      },
      source: {
        value: native,
      },
      data: {
        value: {
          ...native.envelope,
          event,
          ...extra,
        },
      },
    });
    env.window.dispatchEvent(message);
  };
  native.respond = (message, values = {}) => {
    const event = new env.window.Event('message');
    Object.defineProperties(event, {
      origin: {
        value: env.window.location.origin,
      },
      source: {
        value: native,
      },
      data: {
        value: {
          protocol: message.protocol,
          game: message.game,
          epoch: message.epoch,
          request: message.request,
          ok: true,
          ...values,
        },
      },
    });
    env.window.dispatchEvent(event);
  };
  native.postMessage = (message, target) => {
    assert.equal(target, env.window.location.origin);
    owner.outbound.push(message);
    owner.order.push(message.command);
    if (!message.request) return;
    if (message.command === 'sync' && owner.syncMode === 'hold') {
      owner.pendingSync.push(message);
      return;
    }
    if (message.command === 'write' && owner.writeMode === 'hold') {
      owner.pendingWrite.push(message);
      return;
    }
    if (message.command === 'list' && owner.holdImportedList && owner.storedFiles.size) {
      owner.pendingList.push(message);
      return;
    }
    if (message.command === 'write' && owner.writeMode !== 'fail')
      owner.storedFiles.set(message.path, [...message.bytes]);
    const response =
      message.command === 'configure' && owner.failConfigure
        ? {
            ok: false,
            error: 'Synthetic native configure failure',
          }
        : message.command === 'sync' && owner.syncMode === 'fail'
          ? {
              ok: false,
              error: 'Synthetic native sync failure',
            }
          : message.command === 'write' && owner.writeMode === 'fail'
            ? {
                ok: false,
                error: 'Synthetic native write failure',
              }
            : message.command === 'read'
              ? {
                  bytes: owner.corruptRead
                    ? [255]
                    : (owner.storedFiles.get(message.path) ?? [3, 4, 5]),
                }
              : message.command === 'list'
                ? {
                    files: [...owner.storedFiles].map(([path, bytes]) => ({
                      path,
                      size: bytes.length,
                    })),
                  }
                : {};
    queueMicrotask(() => native.respond(message, response));
  };
  owner.native = native;
  owner.frame = frame;
  return native;
}
function siteManifest(mode = 'hosted') {
  const hash = createHash('sha256')
    .update(new Uint8Array([1, 2, 3]))
    .digest('hex');
  return {
    schema: 'eagler-touhou/site-manifest/1',
    protocol: 'eagler-touhou/1',
    profile: 'web-validation-test',
    shared:
      mode === 'import'
        ? {
            resourceMode: mode,
          }
        : {
            resourceMode: mode,
            vanillaFont: 'msgothic.ttc',
            unicodeFont: 'unifont.otf',
          },
    games: {
      th06: {
        runtime: 'runtime/th06/th06.html',
        multiplayerRuntime: 'runtime/th06mp/th06.html',
        gameData: {
          path: 'th06.data',
          bytes: 3,
          sha256: hash,
          version: `sha256-${hash}`,
          layout: `sha256-${'b'.repeat(64)}`,
        },
        ...(mode === 'import'
          ? {
              offlineCompatibility: {
                schema: 'eagler-touhou/offline-game-pack/1',
                runtimeCompatibility: {
                  protocol: 'eagler-touhou/1',
                  dataLayout: `sha256-${'b'.repeat(64)}`,
                  versionSource: 'offline-pack',
                },
                requiredShared: ['/msgothic.ttc', '/unifont.otf'],
                languages: {
                  source: 'offline-pack',
                  baseline: ['ja'],
                },
              },
            }
          : {}),
        languages: [],
        languageOptions: [
          {
            id: 'ja',
            title: '日本語',
            pack: null,
          },
        ],
        music: {
          midi: {
            files: [],
          },
        },
      },
    },
  };
}
async function mountBrowser({
  initial = '/?game=th06&keep=a%20b',
  autoReady = true,
  strictMode = false,
  blockStorage = false,
  resourceMode = 'hosted',
  noticeEnabled = false,
  hydrate = false,
  host = siteManifest(resourceMode),
  holdHost = false,
  holdFirstUse = false,
  seedStorage = () => {},
  seedPackages = [],
  socketBoundary = false,
  directoryTransport = false,
  actualHistory = false,
  fetchBoundary = () => null,
} = {}) {
  api.validateSiteManifest(host);
  api.validateReleaseCatalog({
    schema: 'eagler-touhou/release-catalog/1',
    games: {},
  });
  env.errors.length = 0;
  env.window.history.replaceState(null, '', initial);
  const storage = new MemoryStorage(),
    sessionStorage = new MemoryStorage();
  storage.values.set('eagler-touhou-first-use-notice-seen-v1', '1');
  storage.values.set('eagler-touhou-site-notice-enabled-v1', noticeEnabled ? '1' : '0');
  storage.values.set('eagler-touhou-less-motion-v1', '1');
  storage.values.set('browser-warning-dismissed', '1');
  storage.values.set('eagler-touch-help-seen-v8', '1');
  storage.values.set(
    api.gamePreferenceStorageKey('th06'),
    JSON.stringify({
      music: 'none',
      options: {
        touchEnabled: false,
        thpracEnabled: false,
      },
    }),
  );
  seedStorage(storage, sessionStorage);
  storage.blocked = blockStorage;
  Object.defineProperty(env.window, 'localStorage', {
    configurable: true,
    value: storage,
  });
  expose('localStorage', storage);
  Object.defineProperty(env.window, 'sessionStorage', {
    configurable: true,
    value: sessionStorage,
  });
  expose('sessionStorage', sessionStorage);
  const idb = new IDBFactory();
  expose('IDBKeyRange', IDBKeyRange);
  Object.defineProperty(env.window, 'indexedDB', {
    configurable: true,
    value: idb,
  });
  expose('indexedDB', idb);
  for (const game of seedPackages)
    await api.installParsedPackageZip(await api.parsePackageZip(importPackageFile(game)));
  installSocketBoundary(socketBoundary, directoryTransport);
  const fetches = [],
    order = [];
  let releaseHost;
  const hostReady = holdHost
    ? new Promise((resolve) => {
        releaseHost = resolve;
      })
    : Promise.resolve();
  let releaseFirstUse;
  const firstUseReady = holdFirstUse
    ? new Promise((resolve) => {
        releaseFirstUse = resolve;
      })
    : Promise.resolve();
  const fetchImpl = async (input, init = {}) => {
    const url = new URL(String(input), env.window.location.href);
    fetches.push({
      url: url.href,
      init,
    });
    const handled = fetchBoundary(url, init);
    if (handled) return handled;
    if (url.pathname.endsWith('/site-manifest.json')) {
      // The document Launcher starts first; the independent directory request
      // can finish while its metadata response is still outstanding.
      if (
        holdHost !== 'launcher' ||
        fetches.filter((item) => new URL(item.url).pathname.endsWith('/site-manifest.json'))
          .length === 1
      )
        await hostReady;
      return Response.json(host);
    }
    if (url.pathname.endsWith('/release-catalog.json'))
      return Response.json({
        schema: 'eagler-touhou/release-catalog/1',
        games: {},
      });
    if (url.pathname.endsWith('/NOTICE.txt')) return new Response('');
    if (url.pathname.endsWith('/content/FIRST_USE_NOTICE.html')) {
      await firstUseReady;
      return new Response(
        await readFile(resolve(project, 'public/content/FIRST_USE_NOTICE.html'), 'utf8'),
      );
    }
    if (url.pathname.endsWith('/content/MULTIPLAYER.html'))
      return new Response(
        await readFile(resolve(project, 'public/content/MULTIPLAYER.html'), 'utf8'),
      );
    throw new Error(`Unexpected fixture network ${url.pathname}`);
  };
  env.window.fetch = fetchImpl;
  expose('fetch', fetchImpl);
  // Original public/index.html bootstrap mark semantics; this fixture does not
  // emulate its critical-visual/preload rendering or emergency UI.
  const boot = {
    done: false,
    stage: 'fixture',
    mark(stage) {
      if (boot.done) return;
      boot.stage = String(stage || boot.stage);
    },
    ready() {
      boot.done = true;
      order.push('boot-ready');
    },
    fail() {
      assert.fail('Original bootstrap failure is unexpected');
    },
  };
  env.window.__eaglerBoot = boot;
  mounted = {
    storage,
    sessionStorage,
    sockets: [],
    scrolls: [],
    order,
    fetches,
    releaseHost,
    releaseFirstUse,
    autoReady,
    syncMode: 'auto',
    pendingSync: [],
    writeMode: 'auto',
    pendingWrite: [],
    pendingList: [],
    holdImportedList: false,
    storedFiles: new Map(),
    corruptRead: false,
    outbound: [],
    navigations: [],
    nativeWrites: [],
    dataReads: [],
    downloads: [],
    fullscreen: null,
    routeEvents: [],
    warnings: [],
  };
  const element = React.createElement(api.BrowserLauncher);
  const routes = [
    {
      path: '*',
      element: strictMode ? React.createElement(React.StrictMode, null, element) : element,
    },
  ];
  // Opt-in address integration for actual directory→room navigation. The
  // production session reads window.location after a committed Router change.
  const router = actualHistory
    ? createBrowserRouter(routes, {
        window: env.window,
      })
    : createMemoryRouter(routes, {
        initialEntries: ['/?keep=a%20b', initial],
      });
  router.subscribe((state) =>
    mounted.routeEvents.push({
      action: state.historyAction,
      location: state.location,
    }),
  );
  const container = env.document.createElement('div');
  env.document.body.append(container);
  const tree = React.createElement(RouterProvider, {
    router,
  });
  let root;
  if (hydrate) {
    container.innerHTML = renderToString(tree);
    mounted.initialNodes = [...container.querySelectorAll('[id]')];
    assert.ok(
      container.querySelector('.game[data-game=th08]'),
      'actual SSR entry retains original cards before hydration',
    );
    assert.equal(container.querySelector('iframe'), null, 'initial presentation owns no Runtime');
    Object.assign(mounted, {
      router,
    });
    await React.act(async () => {
      root = hydrateRoot(container, tree);
      mounted.root = root;
    });
  } else {
    root = createRoot(container);
    Object.assign(mounted, {
      root,
      router,
    });
    await React.act(async () => {
      root.render(tree);
    });
  }
  await until(
    () =>
      env.document.querySelector('#gameFrame') &&
      (holdHost ||
        (actualHistory && initial.startsWith('/lobby')
          ? api.getTestSession()?.directory.getSnapshot().initialized
          : mounted.order.includes('boot-ready'))),
    'actual browser entry initialized',
  );
  mounted.initialRouteCount = mounted.routeEvents.length;
  return mounted;
}
function node(selector) {
  const value = env.document.querySelector(selector);
  assert.ok(value, `Actual node missing: ${selector}`);
  return value;
}
function playerOpen() {
  return env.document.querySelector('#player')?.getAttribute('aria-hidden') === 'false';
}
function commands() {
  return mounted.outbound.map((value) => value.command);
}
function currentUrl() {
  const loc = mounted.router.state.location;
  return loc.pathname + loc.search + loc.hash;
}
async function tick() {
  await React.act(async () => {
    await new Promise((resolve) => setTimeout(resolve, 20));
  });
}
async function until(predicate, message) {
  const deadline = Date.now() + 2500;
  while (!predicate() && Date.now() < deadline) await tick();
  assert.ok(
    predicate(),
    `${message}; evidence=${JSON.stringify({
      url: currentUrl(),
      playerOpen: playerOpen(),
      routerState: mounted.router.state.location.state,
      order: mounted.order,
      fetches: mounted.fetches.map((x) => x.url),
      status: env.document.querySelector('#status')?.textContent,
      startup: env.document.querySelector('#startupError')?.textContent,
      body: env.document.body.textContent.slice(-1600),
    })}`,
  );
}
async function click(selector) {
  await React.act(async () => {
    node(selector).click();
  });
  await tick();
}
async function back() {
  await React.act(async () => {
    void mounted.router.navigate(-1);
  });
  await tick();
}
async function start() {
  await click('#launch');
  assert.equal(node('#decisionDialog').open, true, 'main no-music warning gates launch');
  assert.equal(playerOpen(), false, 'Player stays closed before original input/music confirmation');
  await click('#decisionConfirm');
}
// Main app6320–6399: only successful import completion hides Player. Earlier
// write/reopen/verification failures and picker cancellation keep its visibility.
// These use the real mounted BrowserSession; only native file replies are faked.
const storageImportCases = [
  {
    kind: 'save',
    game: 'th06',
    name: 'import.dat',
    path: 'score.dat',
    accept: '.dat',
  },
  {
    kind: 'hint',
    game: 'th10',
    name: 'import.txt',
    path: 'hint/hint_user.txt',
    accept: '.txt',
  },
  {
    kind: 'replay',
    game: 'th06',
    name: 'th6_01.rpy',
    path: 'replay/th6_01.rpy',
    accept: '.zip,.rpy,.rpyx',
  },
];
async function runningStorageImport({ kind, game }) {
  const f = await mountBrowser({
    initial: `/?game=${game}&keep=a%20b`,
    host: subsetHost([game], {
      multiplayer: false,
    }),
    seedPackages: game === 'th10' ? [game] : [],
    seedStorage(storage) {
      storage.values.set(
        api.gamePreferenceStorageKey(game),
        JSON.stringify({
          music: 'none',
          options: {
            touchEnabled: false,
            thpracEnabled: false,
          },
        }),
      );
    },
  });
  await start();
  await until(() => commands().includes('launch'), 'real owner is running before import');
  if (kind === 'replay') {
    await click('[data-action="manage-replay"]');
    await until(
      () => node('#replayDialog').open && !env.document.querySelector('.replay-loading'),
      'actual replay manager read the running owner',
    );
  }
  return f;
}
async function openStoragePicker({ kind, accept }) {
  await click(`[data-action="import-${kind}"]`);
  if (kind === 'save') {
    assert.equal(node('#decisionDialog').open, true);
    await click('#decisionConfirm');
  }
  assert.equal(node('#fileInput').accept, accept);
}
async function chooseStorageFile(entry) {
  const input = node('#fileInput');
  Object.defineProperty(input, 'files', {
    configurable: true,
    value: [new NativeFile([new Uint8Array([3, 7, 9])], entry.name)],
  });
  await React.act(async () => {
    input.dispatchEvent(
      new env.window.Event('change', {
        bubbles: true,
      }),
    );
  });
  await tick();
}
// Real development acquisition, paused only at the authored fetch/storage edges.
function developmentAcquisitionHost(ids) {
  const host = subsetHost(ids);
  host.profile = 'web-development';
  for (const game of ids) {
    const product = api.PRODUCT_GAMES[game];
    host.games[game].gameData.path = product.package.dataTarget.slice(1);
    if (!('multiplayerRuntime' in product)) delete host.games[game].multiplayerRuntime;
    if (product.dataProvider === 'retail-memory')
      host.games[game].gameData.source = `fixture-${game}.data`;
  }
  return host;
}
function heldPackageFetch() {
  const requests = new Map(),
    pending = new Map(),
    held = new Set();
  let releasing = false;
  return {
    requests,
    fetchBoundary(url, init) {
      const game = url.pathname.match(/fixture-(th\d{2})\.data$/)?.[1];
      if (!game) return null;
      let release;
      const promise = new Promise((resolve) => {
        release = resolve;
      });
      const request = {
        signal: init.signal,
        released: false,
        release() {
          if (request.released) return;
          request.released = true;
          held.delete(request);
          release(new Response(new Uint8Array([1, 2, 3])));
        },
      };
      requests.set(game, request);
      held.add(request);
      for (const resolve of pending.get(game) ?? []) resolve(request);
      // Teardown can begin before asynchronous Package Store work reaches fetch.
      // Drain both already-held requests and any request registered afterwards.
      if (releasing) request.release();
      return promise;
    },
    async waitForRequest(game, operation, { timeoutMs = 10_000 } = {}) {
      if (requests.has(game)) return requests.get(game);
      let registered, timer;
      const registration = new Promise((resolve) => {
        registered = resolve;
      });
      const listeners = pending.get(game) ?? new Set();
      pending.set(game, listeners);
      listeners.add(registered);
      try {
        return await Promise.race([
          registration,
          new Promise((_, reject) => {
            timer = setTimeout(
              () => reject(new Error(`Package fetch ${game} was not registered`)),
              timeoutMs,
            );
          }),
          ...(operation
            ? [
                operation.then(() => {
                  throw new Error(
                    `Operation completed before package fetch ${game} was registered`,
                  );
                }),
              ]
            : []),
        ]);
      } finally {
        clearTimeout(timer);
        listeners.delete(registered);
        if (!listeners.size) pending.delete(game);
      }
    },
    releaseAll() {
      releasing = true;
      for (const request of held) request.release();
    },
  };
}
function importPackageFile(game = 'th06') {
  const data = new Uint8Array([1, 2, 3]),
    hash = createHash('sha256').update(data).digest('hex');
  const files = Object.fromEntries(
    [
      ['game-data', `${game}.data`, `/${game}.data`],
      ['font', 'msgothic.ttc', '/msgothic.ttc'],
      ['unicode', 'unifont.otf', '/unifont.otf'],
    ].map(([id, source, target]) => [
      id,
      {
        source,
        target,
        revision: `${id}-r1`,
        bytes: data.length,
        sha256: hash,
      },
    ]),
  );
  const descriptor = {
    schema: 'eagler-touhou/package/1',
    game,
    revision: 'synthetic-import-r1',
    runtimeRequirement: {
      protocol: 'eagler-touhou/1',
      target: game,
      dataFile: 'game-data',
      dataLayout: `sha256-${'b'.repeat(64)}`,
    },
    files,
    base: {
      files: ['game-data', 'font', 'unicode'],
    },
    components: {},
  };
  const archive = zipSync(
    {
      'package.json': strToU8(JSON.stringify(descriptor)),
      [`${game}.data`]: data,
      'msgothic.ttc': data,
      'unifont.otf': data,
    },
    {
      level: 0,
    },
  );
  return new NativeFile([archive], `${game}.zip`, {
    type: 'application/zip',
  });
}
async function selectImport(file) {
  await click('#transferImport');
  const input = node('#fileInput');
  assert.equal(input.accept, '.zip,.dat,application/zip');
  Object.defineProperty(input, 'files', {
    configurable: true,
    value: [file],
  });
  await React.act(async () => {
    input.dispatchEvent(
      new env.window.Event('change', {
        bubbles: true,
      }),
    );
  });
  await tick();
}
// These are supplemental owner-integration checks derived from main1493–1521,
// 4840–4845 and9742–9751. The original browser scenarios remain unchanged.
function subsetHost(ids, { multiplayer = true } = {}) {
  const value = siteManifest(),
    template = value.games.th06;
  value.games = Object.fromEntries(
    ids.map((id) => [
      id,
      {
        ...structuredClone(template),
        runtime: `runtime/${id}/${id}.html`,
        multiplayerRuntime: multiplayer ? `runtime/${id}mp/${id}.html` : undefined,
        gameData: {
          ...template.gameData,
          path: `${id}.data`,
        },
      },
    ]),
  );
  return value;
}
async function acceptHeldHost(f) {
  await React.act(async () => {
    f.releaseHost();
  });
  await until(() => f.order.includes('boot-ready'), 'first real Host finished');
}
function visibleOptions() {
  return env.document.querySelector('.tools[aria-hidden="false"]');
}
function capturePreparationTimers() {
  const originalSet = globalThis.setTimeout,
    originalClear = globalThis.clearTimeout,
    timers = [];
  globalThis.setTimeout = (callback, delay, ...args) => {
    if (![10_000, 20_000, 120_000].includes(delay)) return originalSet(callback, delay, ...args);
    const id = originalSet(() => {}, 2_147_483_647);
    id.unref?.();
    timers.push({
      id,
      delay,
      callback: () => callback(...args),
      active: true,
    });
    return id;
  };
  globalThis.clearTimeout = (id) => {
    const timer = timers.find((value) => value.id === id);
    if (timer) timer.active = false;
    originalClear(id);
  };
  return {
    live: (delay) => timers.filter((value) => value.delay === delay && value.active),
    fire(timer) {
      assert.ok(timer?.active, 'only an active original timer can fire');
      timer.active = false;
      originalClear(timer.id);
      timer.callback();
    },
    restore() {
      globalThis.setTimeout = originalSet;
      globalThis.clearTimeout = originalClear;
      for (const timer of timers) originalClear(timer.id);
    },
  };
}
async function liveActualDirectory({ strictMode = false } = {}) {
  const host = subsetHost(['th06', 'th07']);
  host.shared.netplayRelay = 'wss://relay.invalid/socket';
  const f = await mountBrowser({
    initial: '/lobby.html?game=th06mp',
    host,
    socketBoundary: true,
    directoryTransport: true,
    actualHistory: true,
    strictMode,
    seedPackages: ['th06', 'th07'],
  });
  const session = api.getTestSession();
  await React.act(async () => {
    session.setLocale('en');
  });
  await until(
    () => f.sockets.some((socket) => new URL(socket.url).searchParams.has('directory')),
    'actual directory transport exists',
  );
  const directorySocket = f.sockets.findLast((socket) =>
    new URL(socket.url).searchParams.has('directory'),
  );
  await React.act(async () => {
    directorySocket.open();
    directorySocket.message({
      type: 'directory',
      version: 1,
      product: 'th06mp',
      rooms: [],
      total: 0,
    });
  });
  return {
    f,
    session,
    directorySocket,
  };
}
// Main app747–774 owns terminal lobby closure. Execute that pinned listener,
// then compare the actual document-lived session, BrowserRouter and feedback.
// Transport/Runtime endpoints remain synthetic; no native browser is exercised.
const terminalPinnedMain = 'edee9633e5e3ee79cd2e1aa334f84f6caf755090';
async function actualTerminalRoom({ fromDirectory, locale = 'en', strictMode = false } = {}) {
  const host = siteManifest();
  host.shared.netplayRelay = 'wss://relay.invalid/socket';
  const f = await mountBrowser({
    initial: `${locale === 'en' ? '/en.html' : '/'}?game=th06mp&mpRoom=1234${fromDirectory ? '&fromLobby=1' : ''}`,
    host,
    socketBoundary: true,
    directoryTransport: true,
    actualHistory: true,
    strictMode,
    seedPackages: ['th06'],
    seedStorage(storage) {
      storage.values.set('eagler-touhou-ui-locale-v1', locale);
    },
  });
  const session = api.getTestSession();
  await React.act(async () => {
    session.setLocale(locale);
  });
  await until(
    () => f.sockets.some((value) => new URL(value.url).searchParams.has('lobby')),
    'actual room transport exists before terminal closure',
  );
  const socket = f.sockets.find((value) => new URL(value.url).searchParams.has('lobby'));
  await React.act(async () => {
    socket.open();
    socket.message({
      type: 'state',
      roomDirectory: {
        version: 1,
        controlModes: true,
      },
      room: {
        code: '1234',
        playerCount: 2,
        difficulty: 1,
        phase: 'lobby',
        inputDelay: 0,
        predictionLimit: 8,
        startSerial: 0,
        seats: [
          {
            clientId: new URL(socket.url).searchParams.get('lobby'),
            name: 'A',
            loadout: 0,
            ready: false,
          },
          null,
        ],
        spectators: [],
      },
    });
  });
  await until(
    () => session.room.service.getSnapshot().preparation?.status === 'ready',
    'canonical local resources are ready',
  );
  return {
    f,
    session,
    socket,
  };
}
function captureTerminalFeedback(session) {
  const initial = session.feedback.getSnapshot(),
    toasts = [];
  let revision = initial.toastRevision;
  const unsubscribe = session.feedback.subscribe(() => {
    const value = session.feedback.getSnapshot();
    if (value.toastRevision !== revision) {
      revision = value.toastRevision;
      toasts.push(value.toast);
    }
  });
  return {
    initial,
    toasts,
    unsubscribe,
  };
}

export {
  NativeFile,
  React,
  acceptHeldHost,
  actualTerminalRoom,
  api,
  assert,
  back,
  capturePreparationTimers,
  captureTerminalFeedback,
  chooseStorageFile,
  click,
  commands,
  currentUrl,
  developmentAcquisitionHost,
  env,
  heldPackageFetch,
  importPackageFile,
  liveActualDirectory,
  mountBrowser,
  node,
  openStoragePicker,
  playerOpen,
  runningStorageImport,
  selectImport,
  siteManifest,
  start,
  storageImportCases,
  subsetHost,
  test,
  tick,
  until,
  visibleOptions,
};
