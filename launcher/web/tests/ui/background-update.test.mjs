/** Background and foreground update ownership using explicit package and runtime ports. */
import test, { before, after } from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, rm } from 'node:fs/promises';
import { resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { build, transform } from 'esbuild';
import { authoredSourcesPlugin } from './authored-sources.mjs';
const project = fileURLToPath(new URL('../../', import.meta.url));
const baseUrl = 'https://launcher.invalid/deploy/';
let work, api;
before(async () => {
  await mkdir(resolve(project, '.cache'), {
    recursive: true,
  });
  work = await mkdtemp(resolve(project, '.cache/ui-background-parity-'));
  const outfile = resolve(work, 'actual-services.mjs');
  await build({
    absWorkingDir: project,
    stdin: {
      resolveDir: project,
      loader: 'ts',
      contents: `
    export {createPackageAcquisition} from './app/services/package-acquisition.ts';
    export {startPreparedGameBackground} from './app/services/game-preparation-background.ts';
    export {componentFileIds} from './package/package-generation.mjs';
    export {translate} from './app/i18n.tsx';
  `,
    },
    outfile,
    bundle: true,
    platform: 'node',
    format: 'esm',
    packages: 'external',
    logLevel: 'silent',
    plugins: [authoredSourcesPlugin(project)],
  });
  api = await import(pathToFileURL(outfile).href);
});
after(async () => {
  if (work)
    await rm(work, {
      recursive: true,
      force: true,
    });
});
const actualTranslate = (locale) => (key, params) => api.translate(locale, key, params);
function oldGeneration() {
  const ids = ['game-data', 'track-1', 'track-2', 'track-3'];
  return {
    id: 'installed-old',
    game: 'th06',
    descriptor: {
      schema: 'eagler-touhou/package/1',
      game: 'th06',
      revision: 'old',
      runtimeRequirement: {
        protocol: 'eagler-touhou/1',
        target: 'th06',
        dataFile: 'game-data',
      },
      files: Object.fromEntries(
        ids.map((id) => [
          id,
          {
            source: id === 'game-data' ? 'th06.data' : `${id}.ogg`,
            target: id === 'game-data' ? '/th06.data' : `/bgm/${id}.ogg`,
            revision: `${id}-r1`,
            bytes: 3,
            sha256: 'a'.repeat(64),
          },
        ]),
      ),
      base: {
        files: ['game-data'],
      },
      components: {
        ogg: {
          type: 'ogg',
          files: ids.slice(1),
        },
      },
    },
    files: Object.fromEntries(
      ids.slice(0, 3).map((id) => [
        id,
        {
          objectId: `object-${id}`,
          revision: `${id}-r1`,
        },
      ]),
    ),
  };
}
function publication(revision) {
  return {
    schema: 'eagler-touhou/release-catalog/1',
    games: {
      th06: {
        revision,
        descriptor: 'th06.package.json',
      },
    },
  };
}
function feedback() {
  const visible = [],
    diagnostics = [],
    status = [],
    activity = [],
    decisions = [];
  return {
    visible,
    diagnostics,
    status,
    activity,
    decisions,
    warn(message, error) {
      diagnostics.push([message, error.message]);
    },
    onBackgroundError(error, message) {
      diagnostics.push([message, error.message]);
    },
    toast(message) {
      visible.push(message);
    },
  };
}
function actualFixture({ generation, catalog, locale, source = 'local', install }) {
  const current = {
      generation,
      installation: {
        source,
      },
    },
    metadata = {
      siteManifest: null,
      releaseCatalog: catalog,
    };
  let refreshed = 0;
  const acquisition = api.createPackageAcquisition({
    baseUrl,
    translate: actualTranslate(locale),
    metadata: {
      getSnapshot: () => metadata,
      initialize: async () => {
        throw new Error('Installed launch must not wait for metadata');
      },
      refreshInstalled: async () => {
        refreshed++;
      },
    },
    dependencies: {
      readCurrent: async () => current,
      installPublished: install,
    },
  });
  return {
    acquisition,
    current,
    get refreshed() {
      return refreshed;
    },
  };
}
for (const locale of ['zh-CN', 'en']) {
  test(`background Package failure stays diagnostic-only and keeps current (${locale})`, async () => {
    for (const source of ['local', 'remote'])
      for (const cancelled of [false, true]) {
        const error = cancelled
          ? new DOMException('fixture cancelled update', 'AbortError')
          : new Error('fixture update fetch failed');
        const generation = oldGeneration(),
          catalog = publication('new'),
          actual = feedback();
        let actualInstalls = 0;
        const fixture = actualFixture({
          generation,
          catalog,
          locale,
          source,
          install: async (_game, options) => {
            actualInstalls++;
            assert.equal(options.signal, undefined);
            throw error;
          },
        });
        const acquired = await fixture.acquisition.acquire({
          productId: 'th06',
          music: 'none',
          language: 'ja',
          decideUpdate: async () => 'secondary',
          onWarning: actual.toast,
          onUpdateActivity: (value) => actual.activity.push(value),
        });
        assert.equal(acquired.generation, generation);
        assert.equal(acquired.deferredUpdate, fixture.current);
        assert.equal(
          actualInstalls,
          0,
          'Choosing background does not install before post-launch background work',
        );
        await api.startPreparedGameBackground({
          input: {
            productId: 'th06',
            settings: {
              language: 'ja',
            },
            acquisition: fixture.acquisition,
            translate: actualTranslate(locale),
            runtime: {
              getSnapshot: () => ({
                epoch: 1,
                launched: true,
                game: 'th06',
                music: 'none',
              }),
            },
            onBackgroundError: actual.onBackgroundError,
            onWarning: actual.toast,
          },
          prepared: {
            music: 'none',
            plan: {
              game: 'th06',
              generation,
            },
          },
          epoch: 1,
          deferredUpdate: acquired.deferredUpdate,
        });
        assert.equal(actual.diagnostics.length, 1);
        assert.equal(actual.diagnostics[0][1], error.message);
        assert.deepEqual(actual.visible, []);
        assert.deepEqual(
          actual.activity,
          [],
          'Background work never opens foreground cancellation',
        );
        assert.equal(actualInstalls, 1);
        assert.equal((await fixture.acquisition.readCurrent('th06')).generation, generation);
        assert.equal(
          fixture.refreshed,
          0,
          'A failed transaction cannot publish a replacement generation',
        );
      }
  });
  test(`progressive OGG failure has one live translated toast and no retired feedback (${locale})`, async () => {
    for (const retirement of ['live', 'closed', 'replaced']) {
      const generation = oldGeneration(),
        catalog = publication('old'),
        error = new Error('fixture third-track fetch failed');
      const actual = feedback();
      let snapshot = {
        epoch: 1,
        launched: true,
        game: 'th06',
        music: 'ogg',
      };
      const fixture = actualFixture({
        generation,
        catalog,
        locale,
        install: async (_game, options) => {
          assert.deepEqual(options.addFileIds, ['track-3']);
          if (retirement === 'closed')
            snapshot = {
              ...snapshot,
              launched: false,
            };
          if (retirement === 'replaced')
            snapshot = {
              ...snapshot,
              epoch: 2,
            };
          throw error;
        },
      });
      await api.startPreparedGameBackground({
        input: {
          productId: 'th06',
          settings: {
            language: 'ja',
          },
          acquisition: fixture.acquisition,
          translate: actualTranslate(locale),
          runtime: {
            getSnapshot: () => snapshot,
            extendOggResources: async () => assert.fail('Failed OGG cannot reach Runtime FS'),
          },
          onBackgroundError: actual.onBackgroundError,
          onWarning: actual.toast,
        },
        prepared: {
          music: 'ogg-stream',
          plan: {
            game: 'th06',
            generation,
          },
        },
        epoch: 1,
        deferredUpdate: null,
      });
      assert.equal(actual.diagnostics.length, 1, retirement);
      assert.equal(actual.diagnostics[0][1], error.message);
      if (retirement === 'live') assert.ok(actual.visible[0].includes(error.message));
      assert.equal(actual.visible.length, retirement === 'live' ? 1 : 0);
      assert.equal((await fixture.acquisition.readCurrent('th06')).generation, generation);
      assert.equal(fixture.refreshed, 0);
    }
  });
  test(`foreground failure/cancel keeps its visible warning; declining stays quiet (${locale})`, async () => {
    for (const choice of ['confirm', 'cancel'])
      for (const cancelled of [false, true]) {
        const error = cancelled
          ? new DOMException('fixture cancelled update', 'AbortError')
          : new Error('fixture foreground update failed');
        const generation = oldGeneration(),
          catalog = publication('new'),
          actual = feedback();
        let actualInstalls = 0;
        const fixture = actualFixture({
          generation,
          catalog,
          locale,
          install: async () => {
            actualInstalls++;
            throw error;
          },
        });
        const acquired = await fixture.acquisition.acquire({
          productId: 'th06',
          music: 'none',
          language: 'ja',
          decideUpdate: async (options) => {
            actual.decisions.push(options);
            return choice;
          },
          onWarning: actual.toast,
          onUpdateActivity: (value) => actual.activity.push(value),
          onProgress: (value) => actual.status.push(value.message),
        });
        assert.equal(actual.decisions.length, 1);
        assert.equal(
          new Set(Object.values(actual.decisions[0])).size,
          4,
          'Each available decision has distinct copy',
        );
        assert.equal(actual.visible.length, choice === 'confirm' ? 1 : 0);
        if (choice === 'confirm' && !cancelled)
          assert.ok(actual.visible[0].includes(error.message));
        assert.equal(actual.status.length, choice === 'confirm' ? 1 : 0);
        assert.deepEqual(actual.activity, choice === 'confirm' ? [true, false] : []);
        assert.equal(actualInstalls, choice === 'confirm' ? 1 : 0);
        assert.equal(acquired.generation, generation);
        assert.equal(acquired.deferredUpdate, null);
      }
  });
}
