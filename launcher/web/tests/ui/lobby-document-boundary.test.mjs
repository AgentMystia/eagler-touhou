/** Match current stylesheet ownership against mounted production components.
 * These checks cover DOM/selector boundaries; browser geometry is tested separately. */
import test, { before, after, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { mkdir, mkdtemp, rm } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import { resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { build } from 'esbuild';
import postcss from 'postcss';
import { installMountedDom } from './mounted-dom-environment.mjs';
import { authoredSourcesPlugin } from './authored-sources.mjs';
const project = fileURLToPath(new URL('../../', import.meta.url));
const read = (path) => readFileSync(resolve(project, path), 'utf8').replaceAll('\r\n', '\n');
const stylesheet = postcss.parse(read('public/lobby.css'));
const focusRules = [], motionRules = [];
stylesheet.walkRules(candidate => {
  if (candidate.selector.includes(':focus-visible') && candidate.nodes.some(node => node.prop === 'outline'))
    focusRules.push(candidate.selector);
  if (candidate.nodes.some(node => node.prop === 'animation' && node.value === 'none') &&
      candidate.nodes.some(node => node.prop === 'transition' && node.value === 'none'))
    motionRules.push(candidate.selector);
});
const matches = (node, selector) => node.matches(selector.replaceAll(':focus-visible', ''));
const receivesFocus = node => focusRules.some(selector => matches(node, selector));
const receivesReducedMotion = node => motionRules.some(selector => matches(node, selector));
let env,
  work,
  api,
  React,
  createRoot,
  root,
  disposables = [];
before(async () => {
  env = installMountedDom();
  React = await import('react');
  ({ createRoot } = await import('react-dom/client'));
  await mkdir(resolve(project, '.cache'), {
    recursive: true,
  });
  work = await mkdtemp(resolve(project, '.cache/lobby-document-boundary-'));
  const outfile = resolve(work, 'components.mjs');
  await build({
    absWorkingDir: project,
    stdin: {
      resolveDir: project,
      loader: 'ts',
      contents: `
    export {LobbyOptionsHost} from './app/components/launcher/LobbyOptionsHost.tsx';
    export {LibraryCards} from './app/components/launcher/LibraryCards.tsx';
    export {createLibraryProducts} from './app/components/launcher/products.ts';
    export {MainSelect} from './app/components/launcher/MainSelect.tsx';
    export {createMainSelectController} from './app/components/launcher/main-select-controller.ts';
    export {OptionSwitch} from './app/components/settings/TouchSettingsFields.tsx';
    export {PlayerSurface} from './app/components/player/PlayerSurface.tsx';
    export {FullscreenTransient} from './app/components/FullscreenTransient.tsx';
    export {ConfirmationDialog} from './app/components/ConfirmationDialog.tsx';
    export {Feedback} from './app/components/feedback/Feedback.tsx';
    export {StartupError} from './app/components/feedback/StartupError.tsx';
    export {AppleRefreshDialog} from './app/components/notices/AppleRefreshDialog.tsx';
    export {ReplayDialog} from './app/components/notices/ReplayDialog.tsx';
    export {DonationDialog} from './app/components/notices/DonationDialog.tsx';
    export {MultiplayerGuideDialog} from './app/components/notices/MultiplayerGuideDialog.tsx';
    export {NetplayCalibrationReport} from './app/components/NetplayCalibrationReport.tsx';
    export {createNetplayCalibration} from './app/models/netplay-calibration.ts';
    export {createFeedbackModel} from './app/models/feedback.ts';
    export {createStartupErrorModel} from './app/models/startup-error.ts';
    export {LocaleProvider, translate} from './app/i18n.tsx';
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
  if (root) {
    await React.act(async () => root.unmount());
    root = null;
  }
  for (const dispose of disposables.splice(0)) dispose();
  assert.equal(
    env.document.querySelectorAll('.mizuki-select-menu,[data-launcher-transient-host]').length,
    0,
  );
  assert.deepEqual(env.errors.splice(0), []);
  env.document.body.replaceChildren();
  env.document.body.className = '';
  delete env.document.fullscreenElement;
});
after(async () => {
  env?.close();
  if (work)
    await rm(work, {
      recursive: true,
      force: true,
    });
});
const el = (...args) => React.createElement(...args),
  act = (fn) =>
    React.act(async () => {
      await fn();
    });
const n = (selector) => {
  const node = env.document.querySelector(selector);
  assert.ok(node, selector);
  return node;
};
const noop = () => {};
async function mount({ lessMotion = false } = {}) {
  env.document.body.className = `lobby-page${lessMotion ? ' less-motion' : ''}`;
  const timers = {
      setTimeout: () => 1,
      clearTimeout() {},
    },
    t = (key, values) => api.translate('en', key, values);
  const feedback = api.createFeedbackModel(timers),
    error = api.createStartupErrorModel({
      translate: t,
      launched: () => false,
      copyText: async () => true,
      toast: feedback.toast,
    });
  const calibration = api.createNetplayCalibration({
    timers,
    userAgent: 'synthetic',
  });
  disposables.push(feedback.dispose, error.dispose, calibration.dispose);
  const replayState = {
    phase: 'ready',
    rows: [],
    animateRows: false,
  };
  const replay = {
    subscribe: () => noop,
    getSnapshot: () => replayState,
    close: noop,
  };
  const select = (id) =>
    el(
      api.MainSelect,
      {
        id,
        className: 'option-select',
        defaultValue: 'a',
      },
      el(
        'option',
        {
          value: 'a',
        },
        'A',
      ),
      el(
        'option',
        {
          value: 'b',
        },
        'B',
      ),
    );
  const container = env.document.createElement('div');
  env.document.body.append(container);
  root = createRoot(container);
  const render = (foregroundActive = false) =>
    root.render(
      el(
        React.StrictMode,
        null,
        el(
          api.LocaleProvider,
          {
            locale: 'en',
          },
          el(
            'header',
            {
              className: 'ui-language-control',
            },
            select('lobbyLanguage'),
          ),
          el(
            'button',
            {
              id: 'lobbyAction',
              className: 'lobby-button',
            },
            'Lobby',
          ),
          el(api.LibraryCards, {
            products: api.createLibraryProducts(
              ['th06mp'],
              (path) => path,
              (id) => `?game=${id}`,
            ),
            variant: 'lobby',
            interactive: false,
            onSelect: noop,
            onActivate: noop,
          }),
          el(
            api.LobbyOptionsHost,
            {
              open: true,
              foregroundActive,
              onCloseRequest: noop,
            },
            el(
              'aside',
              {
                className: 'tools',
              },
              el(api.OptionSwitch, {
                id: 'settingsSwitch',
                checked: false,
                onChange: noop,
              }),
              select('settingsSelect'),
            ),
          ),
          el(
            api.PlayerSurface,
            {
              open: true,
              editing: false,
              onElement: noop,
            },
            el(api.OptionSwitch, {
              id: 'playerSwitch',
              checked: false,
              onChange: noop,
            }),
          ),
          el(
            api.FullscreenTransient,
            null,
            el(api.ConfirmationDialog, {
              open: false,
              title: 'Confirm',
              message: 'Message',
              confirmText: 'Yes',
              cancelText: 'No',
              onConfirm: noop,
              onCancel: noop,
            }),
            el(api.Feedback, {
              model: feedback,
            }),
            el(api.StartupError, {
              model: error,
            }),
          ),
          el(api.AppleRefreshDialog, {
            open: true,
            onCloseRequest: noop,
          }),
          el(api.ReplayDialog, {
            model: replay,
            open: false,
            onCloseRequest: noop,
            onImportRequest: noop,
            onDropFile: async () => {},
            onImportError: noop,
          }),
          el(api.NetplayCalibrationReport, {
            model: calibration,
            english: true,
            currentNetwork: () => '',
          }),
          el(api.DonationDialog, {
            open: false,
            onCloseRequest: noop,
            assetUrl: (value) => value,
            onArtworkUnavailable: noop,
          }),
          el(api.MultiplayerGuideDialog, {
            open: false,
            onCloseRequest: noop,
            gameId: 'th06',
            contentUrl: 'content/MULTIPLAYER.html',
          }),
        ),
      ),
    );
  await act(() => render());
  return {
    render: (value) => act(() => render(value)),
  };
}
async function openMenu(id) {
  await act(() => n(`#${id}`).closest('.mizuki-select').querySelector('button').click());
  return n('.mizuki-select-menu:not([hidden])');
}
async function fullscreen(target, event = 'fullscreenchange') {
  Object.defineProperty(env.document, 'fullscreenElement', {
    configurable: true,
    value: target,
  });
  await act(() => env.document.dispatchEvent(new env.window.Event(event)));
}
test('real settings, Player and main-only dialog controls escape lobby focus and motion; lobby controls retain both', async () => {
  await mount({
    lessMotion: true,
  });
  for (const selector of [
    '.game-shelf',
    '.lobby-options-document',
    '#player',
    '#appleRefreshDialog',
    '#replayDialog',
    '#netplayCalibrationDialog',
    '[data-launcher-transient-host]',
  ]) {
    const owner = n(selector);
    assert.ok(owner.hasAttribute('data-launcher-document'), selector);
    assert.equal(receivesReducedMotion(owner), false, `${selector} root`);
    for (const button of owner.querySelectorAll('button')) {
      assert.equal(receivesFocus(button), false, `${selector} focus`);
        assert.equal(receivesReducedMotion(button), false, `${selector} motion`);
    }
  }
  for (const selector of ['#lobbyAction', '#lobbyLanguage', '#donationClose', '#mpGuideClose']) {
    const button = n(selector);
    assert.equal(button.closest('[data-launcher-document]'), null);
    assert.equal(receivesFocus(button), true, selector);
    assert.equal(receivesReducedMotion(button), true, selector);
  }
});
test('settings menu retains document ownership in outer dialog, body and fullscreen Player carriers', async () => {
  const mounted = await mount({
    lessMotion: true,
  });
  const settings = await openMenu('settingsSelect');
  assert.equal(settings.parentElement, n('#lobbyOptionsDialog'));
  assert.ok(settings.hasAttribute('data-launcher-document'));
  assert.equal(settings.closest('.lobby-options-document'), null);
  for (const node of [settings, settings.querySelector('button')])
    assert.equal(receivesReducedMotion(node), false);
  assert.equal(receivesFocus(settings.querySelector('button')), false);
  await mounted.render(true); // Original outer modal is suspended for Player.
  await act(() => n('#settingsSelect').closest('.mizuki-select').querySelector('button').click()); // close
  const bodyMenu = await openMenu('settingsSelect');
  assert.equal(bodyMenu, settings);
  assert.equal(settings.parentElement, env.document.body);
  await fullscreen(n('#player'));
  await act(() => n('#settingsSelect').closest('.mizuki-select').querySelector('button').click()); // close before opening in the new host
  const playerMenu = await openMenu('settingsSelect');
  assert.equal(playerMenu, settings);
  assert.equal(settings.parentElement, n('#player'));
  assert.ok(settings.hasAttribute('data-launcher-document'));
  await fullscreen(null);
  const lobbyMenu = await openMenu('lobbyLanguage');
  assert.equal(lobbyMenu.parentElement, env.document.body);
  assert.equal(lobbyMenu.hasAttribute('data-launcher-document'), false);
  assert.equal(receivesFocus(lobbyMenu.querySelector('button')), true);
  assert.equal(receivesReducedMotion(lobbyMenu), true);
});
test('menu ownership is recomputed when a native select changes document owner', () => {
  const owner = env.document.createElement('div');
  owner.dataset.launcherDocument = '';
  const select = env.document.createElement('select');
  select.innerHTML = '<option>A</option>';
  owner.append(select);
  env.document.body.append(owner);
  const controller = api.createMainSelectController({
    translate: (key) => key,
  });
  try {
    controller.installCustomSelect(select);
    select.parentElement.querySelector('button').click();
    const menu = n('.mizuki-select-menu'),
      wrapper = select.parentElement;
    assert.ok(menu.hasAttribute('data-launcher-document'));
    controller.closeOtherCustomSelects();
    env.document.body.append(wrapper);
    wrapper.querySelector('button').click();
    assert.equal(n('.mizuki-select-menu'), menu);
    assert.equal(menu.hasAttribute('data-launcher-document'), false);
    owner.append(wrapper);
    controller.syncAllCustomSelects();
    assert.ok(menu.hasAttribute('data-launcher-document'));
  } finally {
    controller.dispose();
  }
});
test('fullscreen transient carrier and original dialog nodes keep ownership and identity on both fullscreen events', async () => {
  await mount({
    lessMotion: true,
  });
  const host = n('[data-launcher-transient-host]'),
    decision = n('#decisionDialog'),
    toast = n('#toast'),
    error = n('#startupError');
  for (const [target, event] of [
    [n('#player'), 'fullscreenchange'],
    [null, 'webkitfullscreenchange'],
    [n('#player'), 'webkitfullscreenchange'],
    [null, 'fullscreenchange'],
  ]) {
    await fullscreen(target, event);
    assert.equal(host.parentElement, target || env.document.body);
    assert.ok(host.hasAttribute('data-launcher-document'));
    for (const node of [decision, toast, error]) {
      assert.equal(n(`#${node.id}`), node);
      assert.equal(node.closest('[data-launcher-transient-host]'), host);
      assert.equal(receivesReducedMotion(node), false);
    }
    assert.equal(receivesFocus(n('#decisionConfirm')), false);
  }
});
