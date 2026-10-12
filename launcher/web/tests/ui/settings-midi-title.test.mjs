/** Original app.mts4415–4423 MIDI help/disabled presentation and
 * public/index.html447/544 music labels after original static translations.
 * Synthetic DOM and fake device availability only; no browser permission,
 * native MIDI, rendered tooltip or gameplay claim. */
import test, { before, after, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFile, mkdir, mkdtemp, rm } from 'node:fs/promises';
import { resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { build } from 'esbuild';
import { authoredSourcesPlugin } from './authored-sources.mjs';
import { installMountedDom } from './mounted-dom-environment.mjs';
const project = fileURLToPath(new URL('../../', import.meta.url));
const baseline = 'edee9633e5e3ee79cd2e1aa334f84f6caf755090';
let env, React, createRoot, owners, work, root;
const step = (callback) =>
  React.act(async () => {
    callback?.();
    await Promise.resolve();
  });
before(async () => {
  env = installMountedDom();
  React = await import('react');
  ({ createRoot } = await import('react-dom/client'));
  await mkdir(resolve(project, '.cache'), {
    recursive: true,
  });
  work = await mkdtemp(resolve(project, '.cache/settings-midi-title-'));
  const outfile = resolve(work, 'owners.mjs');
  await build({
    absWorkingDir: project,
    stdin: {
      resolveDir: project,
      loader: 'ts',
      contents: `
    export {SettingsBody} from './app/components/settings/SettingsBody.tsx';
    export {createGameSettingsModel} from './app/models/game-settings.ts';
    export {LocaleProvider, translate} from './app/i18n.tsx';
    export {setUiLocale, applyStaticTranslations} from './src/launcher/i18n.mts';
  `,
    },
    outfile,
    bundle: true,
    platform: 'node',
    format: 'esm',
    jsx: 'automatic',
    packages: 'external',
    plugins: [authoredSourcesPlugin(project)],
    logLevel: 'silent',
  });
  owners = await import(pathToFileURL(outfile).href);
});
afterEach(async () => {
  if (root) await step(() => root.unmount());
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
for (const locale of ['zh-CN', 'en'])
  for (const productId of ['th06', 'th06mp'])
    test(`${locale} ${productId}: music label and MIDI title/disabled follow main across locale/support/visibility`, async () => {
      owners.setUiLocale(locale, {
        persist: false,
        notify: false,
      });
      const musicId = productId.endsWith('mp') ? 'mpMusicSelect' : 'musicSelect';
      const values = new Map(),
        storage = {
          getItem: (key) => values.get(key) ?? null,
          setItem: (key, value) => values.set(key, String(value)),
        };
      const model = owners.createGameSettingsModel({
        storage,
      });
      model.hydrate({
        productId,
        uiLocale: locale,
        languages: [
          {
            id: 'ja',
            title: '日本語',
          },
        ],
        musicAvailability: {
          audio: true,
          midiAvailable: true,
          importServer: false,
          remoteOggAdvertised: false,
        },
        webMidiAvailable: false,
        mobile: false,
      });
      const listeners = new Set(),
        calls = [];
      let midi = Object.freeze({
        enabled: false,
        granted: false,
        busy: false,
        supported: false,
        outputs: [],
        selectedId: '',
        hint: owners.translate(locale, 'settings.externalMidiUnsupported'),
      });
      const unavailable = () => {
        throw new Error('Unexpected non-MIDI fixture action');
      };
      const actions = {
        confirm: unavailable,
        file: unavailable,
        showAppleNotice: unavailable,
        feedback: unavailable,
        reportError: unavailable,
        externalMidi: {
          subscribe: (callback) => {
            listeners.add(callback);
            return () => listeners.delete(callback);
          },
          getSnapshot: () => midi,
          setEnabled: async (value) => calls.push(value),
          selectOutput: unavailable,
        },
      };
      const container = env.document.createElement('div');
      env.document.body.append(container);
      root = createRoot(container);
      await step(() =>
        root.render(
          React.createElement(
            owners.LocaleProvider,
            {
              locale,
            },
            React.createElement(owners.SettingsBody, {
              model,
              actions,
              onOpenTouchLayout: unavailable,
            }),
          ),
        ),
      );
      assert.equal(
        container.querySelector('.mp-setting-item,.mp-mobile-options,.mp-file-tools-grid'),
        null,
        'common settings do not opt into separate MP spacing',
      );
      const practice = container.querySelector(
        productId.endsWith('mp') ? '#mpThpracToggle' : '#thpracToggle',
      );
      assert.ok(practice, 'practice row exists in both versions');
      assert.equal(practice.disabled, productId.endsWith('mp'));
      const prefix = productId.endsWith('mp') ? 'mpExternalMidi' : 'externalMidi';
      const button = env.document.getElementById(`${prefix}Toggle`);
      const musicSelect = env.document.getElementById(musicId);

      // Original custom-select.mts falls back to common.selectOption when the
      // native select has neither aria-label nor a linked label element.

      await step(() => button.click());
      assert.deepEqual(calls, []);
      await step(() => {
        midi = Object.freeze({
          ...midi,
          supported: true,
          hint: owners.translate(locale, 'settings.externalMidiHint'),
        });
        for (const listener of listeners) listener();
      });
      await step(() => button.click());
      assert.deepEqual(calls, [true]);
      await step(() => model.setMusic('none'));
      await step(() => model.setMusic('midi'));
      await step(() => {
        midi = Object.freeze({
          ...midi,
          supported: false,
          hint: owners.translate(locale, 'settings.externalMidiUnsupported'),
        });
        for (const listener of listeners) listener();
      });
      assert.equal(button.title, owners.translate(locale, 'settings.externalMidiUnsupported'));
    });
