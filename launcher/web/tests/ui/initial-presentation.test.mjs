import { dependencyExecutable } from '../../lib/source-layout.mjs';
/** Current initial document and production artifacts; no physical-browser claim. */
import test, { before, after, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { build } from 'esbuild';
import { authoredSourcesPlugin } from './authored-sources.mjs';
import { installMountedDom } from './mounted-dom-environment.mjs';
const project = fileURLToPath(new URL('../../', import.meta.url));
let work, env, api, React, renderToStaticMarkup, createRoot, MemoryRouter, root;
const el = (type, props, ...children) => React.createElement(type, props, ...children);
const parse = (html) => new env.window.DOMParser().parseFromString(html, 'text/html');
function assertUniqueIds(document) {
  const ids = [...document.querySelectorAll('[id]')].map((node) => node.id);
  assert.equal(new Set(ids).size, ids.length, 'Static document must not include duplicate owners');
}
before(async () => {
  env = installMountedDom();
  React = await import('react');
  ({ renderToStaticMarkup } = await import('react-dom/server'));
  ({ createRoot } = await import('react-dom/client'));
  ({ MemoryRouter } = await import('react-router'));
  work = await mkdtemp(resolve(project, '.cache/initial-presentation-'));
  const outfile = resolve(work, 'actual.mjs');
  await build({
    absWorkingDir: project,
    stdin: {
      resolveDir: project,
      loader: 'ts',
      contents: `
    export {BrowserLauncher} from './app/BrowserLauncher.tsx';
    export * from './app/components/startup/InitialLauncher.tsx';
    export * from '@eagler/contracts/product-catalog';
    export {translate} from './app/i18n.tsx';
  `,
    },
    outfile,
    bundle: true,
    platform: 'node',
    format: 'esm',
    jsx: 'automatic',
    packages: 'external',
    logLevel: 'silent',
    plugins: [authoredSourcesPlugin(project)],
  });
  api = await import(pathToFileURL(outfile).href);
});
afterEach(async () => {
  if (root) await React.act(async () => root.unmount());
  root = null;
  env.document.body.replaceChildren();
  assert.deepEqual(env.errors.splice(0), []);
});
after(async () => {
  env?.close();
  if (work)
    await rm(work, {
      recursive: true,
      force: true,
    });
});
function initialMarkup(path) {
  return renderToStaticMarkup(
    el(
      MemoryRouter,
      {
        initialEntries: [path],
      },
      el(api.BrowserLauncher),
    ),
  );
}
for (const path of [
  '/',
  '/index.html',
  '/en.html',
  '/nested/',
  '/nested/en.html',
  '/?game=th20&test=1',
]) {
  test(`initial document respects the current product catalog without starting a session: ${path}`, () => {
    const document = parse(initialMarkup(path)),
      locale = path.includes('en.html') ? 'en' : 'zh-CN';
    const cards = [...document.querySelectorAll('.game')];
    assert.deepEqual(
      cards.map((card) => card.dataset.product || card.dataset.game).sort(),
      [...api.PRODUCT_IDS].sort(),
    );
    for (const card of cards) {
      const id = card.dataset.product || card.dataset.game;
      assert.equal(card.hidden, !api.productEnabledForBuild(id), id);
      assert.equal(
        new URL(card.getAttribute('href'), 'https://launcher.invalid/').searchParams.get('game'),
        id,
      );
      assert.ok(card.querySelector('h2')?.textContent.trim(), `${id}: accessible game title`);
    }
    assert.equal(
      document.querySelector('#singleplayerHeading').textContent,
      api.translate(locale, 'library.singleplayer'),
    );
    assert.equal(
      document.querySelector('#multiplayerHeading').textContent,
      api.translate(locale, 'library.multiplayer'),
    );
    assert.equal(document.querySelector('#uiLanguageSelect').value, locale);
    assert.equal(document.querySelector('.shelf-lobby-link').getAttribute('href'), 'lobby.html');
    assert.equal(document.querySelector('#originMigrationOpen').hidden, true);
    assert.equal(document.querySelector('iframe'), null);
    assert.equal(document.querySelector('dialog[open]'), null);
    assertUniqueIds(document);
  });
}
for (const path of ['/lobby.html', '/lobby', '/nested/lobby.html?game=th09mp'])
  test(`original empty lobby boot presentation: ${path}`, () => {
    const document = parse(initialMarkup(path));
    assert.equal(document.querySelector('.lobby-main').hasAttribute('inert'), true);
    assert.equal(document.querySelector('#lobbyGameRail').children.length, 0);
    assert.equal(document.querySelector('#filters').children.length, 0);
    assert.equal(document.querySelector('#roomList').children.length, 0);
    for (const id of [
      'createButton',
      'codeButton',
      'refreshRooms',
      'lobbyGuideOpen',
      'lobbyNetworkCheck',
    ]) {
      const actual = document.getElementById(id);
      assert.ok(actual, `${id}: initial control exists`);
      assert.ok(
        actual.disabled || actual.closest('[inert]'),
        `${id}: unavailable before hydration`,
      );
    }
    assert.equal(document.querySelector('#listHead').hidden, true);
    assert.equal(document.querySelector('#listLoading').hidden, false);
    assert.equal(document.querySelector('#roomList').getAttribute('aria-busy'), 'true');
    assert.equal(document.querySelector('.site-footer'), null);
    assert.equal(document.querySelector('.brand'), null);
    assert.equal(document.querySelector('iframe'), null);
    assertUniqueIds(document);
  });
for (const lobby of [false, true])
  test(`${lobby ? 'lobby' : 'library'} noscript provides a readable fallback and local navigation`, () => {
    const output = renderToStaticMarkup(
      el(api.OriginalNoscript, {
        lobby,
      }),
    );
    const document = parse(`<!doctype html><html><head></head><body>${output}</body></html>`);
    const fallback = document.querySelector('noscript');
    assert.ok(fallback, 'fallback does not require JavaScript');
    assert.ok(
      fallback.querySelector('p')?.textContent.trim(),
      'fallback explains the unavailable page',
    );
    const links = [...fallback.querySelectorAll('a[href]')];
    assert.ok(links.length > 0, 'fallback provides navigation instead of a dead end');
    for (const link of links) {
      const target = new URL(link.getAttribute('href'), 'https://launcher.invalid/');
      assert.equal(target.origin, 'https://launcher.invalid');
      assert.ok(link.textContent.trim(), 'navigation has readable link text');
    }
    assert.equal(document.querySelector('script, iframe'), null);
  });
test('initial presentation starts no rail observers/RAF and removes its document handlers on replacement', async () => {
  const activeClicks = new Set(),
    add = env.document.addEventListener,
    remove = env.document.removeEventListener;
  const previousObserver = globalThis.ResizeObserver,
    previousFrame = globalThis.requestAnimationFrame;
  let observers = 0,
    frames = 0;
  env.document.addEventListener = function (type, fn, options) {
    if (type === 'click') activeClicks.add(fn);
    return add.call(this, type, fn, options);
  };
  env.document.removeEventListener = function (type, fn, options) {
    if (type === 'click') activeClicks.delete(fn);
    return remove.call(this, type, fn, options);
  };
  globalThis.ResizeObserver = class {
    constructor() {
      observers++;
    }
    observe() {}
    disconnect() {}
  };
  globalThis.requestAnimationFrame = () => {
    frames++;
    return 1;
  };
  try {
    const container = env.document.createElement('div');
    env.document.body.append(container);
    root = createRoot(container);
    await React.act(async () => root.render(el(MemoryRouter, null, el(api.InitialLauncher))));
    const initial = [...container.querySelectorAll('[id]')];
    assert.ok(initial.some((node) => node.id === 'singleplayerRail'));
    assert.equal(observers, 0);
    assert.equal(frames, 0);
    assert.equal(activeClicks.size, 0);
    await React.act(async () =>
      root.render(
        el('main', {
          id: 'real-session-boundary',
        }),
      ),
    );
    assert.equal(activeClicks.size, 0);
    assert.ok(initial.every((node) => !node.isConnected));
    assertUniqueIds(env.document);
  } finally {
    env.document.addEventListener = add;
    env.document.removeEventListener = remove;
    globalThis.ResizeObserver = previousObserver;
    globalThis.requestAnimationFrame = previousFrame;
  }
});
for (const initialLobby of [false, true])
  test(`document body seed preserves live class owners across library/lobby changes (${initialLobby})`, async () => {
    const document = env.document;
    root = createRoot(document.documentElement);
    const render = (lobby) =>
      el(
        React.Fragment,
        null,
        el('head'),
        el(
          api.InitialDocumentBody,
          {
            lobby,
          },
          el('main', {
            'data-route': lobby ? 'lobby' : 'library',
          }),
        ),
      );
    try {
      await React.act(async () => root.render(render(initialLobby)));
      assert.equal(document.body.classList.contains('lobby-page'), initialLobby);
      document.body.classList.add('less-motion', 'player-active', 'app-shell-activation-pending');
      const body = document.body;
      // The existing LauncherApplication effect is the live token owner. Only
      // its one token changes; the document component must never rewrite it.
      for (const lobby of [!initialLobby, initialLobby, !initialLobby]) {
        document.body.classList.toggle('lobby-page', lobby);
        await React.act(async () => root.render(render(lobby)));
        assert.equal(document.body, body);
        assert.deepEqual(
          [...body.classList].sort(),
          [
            'less-motion',
            'player-active',
            'app-shell-activation-pending',
            ...(lobby ? ['lobby-page'] : []),
          ].sort(),
        );
      }
    } finally {
      await React.act(async () => root.unmount());
      root = null;
      document.documentElement.innerHTML = '<head></head><body></body>';
    }
  });
test(
  'actual root/nested builds publish shared initial cards and route-specific original lobby/noscript',
  {
    timeout: 240000,
  },
  async () => {
    for (const [name, mountPath] of [
      ['root', '/'],
      ['nested', '/preview/'],
    ]) {
      const output = resolve(work, name);
      execFileSync(
        process.execPath,
        [dependencyExecutable('@react-router/dev', 'react-router'), 'build'],
        {
          cwd: project,
          env: {
            ...process.env,
            EAGLER_REACT_BUILD_DIRECTORY: output,
            EAGLER_REACT_MOUNT_PATH: mountPath,
            EAGLER_REACT_APP_SHELL: '',
            EAGLER_REACT_APP_SHELL_ORIGIN: '',
          },
          stdio: 'pipe',
          timeout: 120000,
        },
      );
      for (const entry of ['index.html', 'en.html', 'lobby.html']) {
        const document = parse(await readFile(resolve(output, 'client', entry), 'utf8')),
          lobby = entry === 'lobby.html';
        assert.equal(document.documentElement.lang, entry === 'en.html' ? 'en' : 'zh-CN');
        assert.equal(document.body.classList.contains('lobby-page'), lobby);
        assert.equal(document.documentElement.dataset.bootstrapEntry, lobby ? 'lobby' : 'library');
        assert.equal(
          document.documentElement.getAttribute('data-lobby-boot'),
          lobby ? 'loading' : null,
        );
        if (lobby) {
          assert.equal(document.querySelectorAll('.game').length, 0);
          assert.ok(
            document.querySelector('noscript')?.textContent.trim(),
          );
        }
        assert.equal(document.querySelector('iframe'), null);
        assertUniqueIds(document);
      }
    }
  },
);
