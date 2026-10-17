/** Real collection/Host executables with synthetic Runtime payloads. */
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { PRODUCT_GAMES } from '../lib/contracts/product-catalog.mjs';
import { SITE_MANIFEST_SCHEMA, validateSiteManifest } from '../lib/contracts/site-manifest.mjs';
import { writeSyntheticRuntimeRelease } from './support/runtime-release-fixture.mjs';
import { verifyRuntimeRelease } from '../lib/runtime-release.mjs';
import { verifyRuntimePublication } from '../lib/runtime-generations.mjs';
import { verifyLegacySiteManifestAlias } from '../compat/site-manifest.mjs';
import { RUNTIME_VARIANTS } from '../lib/runtime-variants.mjs';

const project = fileURLToPath(new URL('../', import.meta.url));
const work = await mkdtemp(resolve(tmpdir(), 'eagler-runtime-variants-'));
const exec = promisify(execFile);
const command = (...args) => exec(process.execPath, args, {cwd: project, maxBuffer: 8 * 1024 * 1024});
const json = async path => JSON.parse(await readFile(path, 'utf8'));
try {
  const games = ['th06', 'th08'];
  const fixtureRoot = resolve(work, 'fixture');
  const fixture = await writeSyntheticRuntimeRelease(fixtureRoot, games);
  const inputs = {};
  for (const game of games) {
    inputs[game] = {};
    for (const {name, field} of RUNTIME_VARIANTS) {
      const runtime = fixture.games[game][field], directory = resolve(work, 'inputs', game, name);
      await mkdir(directory, {recursive: true});
      for (const file of Object.keys(runtime.files)) {
        await mkdir(resolve(directory, file, '..'), {recursive: true});
        await cp(resolve(fixtureRoot, runtime.root, file), resolve(directory, file));
      }
      await writeFile(resolve(directory, 'runtime-files.json'),
        JSON.stringify({schema: 'eagler-touhou/runtime-directory/1', files: runtime.files}));
      if (PRODUCT_GAMES[game].runtimeFileLayout === 'directory' && !runtime.files['manifest.json'])
        await writeFile(resolve(directory, 'manifest.json'),
          JSON.stringify({game, protocol: 'eagler-touhou/1', features: fixture.games[game].features}));
      inputs[game][name] = directory;
    }
  }
  const hash = 'a'.repeat(64);
  const seed = {schema: SITE_MANIFEST_SCHEMA, protocol: 'eagler-touhou/1', profile: 'web-validation-variants',
    shared: {resourceMode: 'hosted', vanillaFont: 'shared/jp.ttf', unicodeFont: 'shared/unicode.ttf'},
    games: Object.fromEntries(games.map(game => [game, {
      runtime: 'runtime/' + game + '/' + game + '.html?v=seed',
      multiplayerRuntime: 'runtime/' + game + '/multiplayer/' + game + '.html?v=seed',
      gameData: {path: game + '.data', bytes: 4, sha256: hash, version: 'sha256-' + hash, layout: fixture.games[game].dataLayout},
      music: {midi: {files: []}}, features: {thprac: false, focusHitbox: false},
    }]))};
  const seedPath = resolve(work, 'seed.json'), featuresPath = resolve(work, 'features.json');
  await writeFile(seedPath, JSON.stringify(seed));
  await writeFile(featuresPath, JSON.stringify({schema: 'eagler-touhou/server-features/1', resourceMode: 'import',
    games: Object.fromEntries(games.map(game => [game, {languages: ['ja'], thprac: false}]))}));

  const selections = [
    {th06: ['normal'], th08: ['normal']},
    {th06: ['multiplayer'], th08: ['multiplayer']},
    {th06: ['normal', 'multiplayer'], th08: ['multiplayer']},
  ];
  for (const [index, selection] of selections.entries()) {
    const release = resolve(work, 'release-' + index), site = resolve(work, 'site-' + index);
    await command('scripts/package-runtime-release.mjs', '--output=' + release, '--games=' + games.join(','),
      ...games.flatMap(game => selection[game].map(name =>
        '--' + game + (name === 'normal' ? '-build=' : '-multiplayer-build=') + inputs[game][name])));
    const collected = await verifyRuntimeRelease(release);
    for (const game of games) {
      assert.deepEqual(RUNTIME_VARIANTS.filter(({field}) => collected.games[game][field]).map(({name}) => name), selection[game]);
    }
    await command('scripts/package-server.mjs', '--output=' + site, '--runtime-release=' + release,
      '--site-manifest=' + seedPath, '--feature-config=' + featuresPath,
      '--profile=web-validation-variants', '--music=midi');
    await command('scripts/verify-server-build.mjs', site);
    const manifest = validateSiteManifest(await json(resolve(site, 'site-manifest.json')));
    const publication = await verifyRuntimePublication(site, manifest);
    await verifyLegacySiteManifestAlias(site, manifest);
    for (const game of games) {
      assert.deepEqual(RUNTIME_VARIANTS.filter(({field}) => manifest.games[game][field]).map(({name}) => name), selection[game]);
    }
    assert.deepEqual(publication.groups.map(group => group.root).sort(), games.flatMap(game =>
      selection[game].map(name => 'runtime/' + game + (name === 'normal' ? '/' : '/multiplayer/'))).sort());
    if (index === 1) {
      await assert.rejects(readFile(resolve(site, 'host-manifest.json')), {code: 'ENOENT'});
      await assert.rejects(readFile(resolve(site, 'runtime/th06/th06.html')), {code: 'ENOENT'});
    } else {
      const legacy = await json(resolve(site, 'host-manifest.json'));
      assert.deepEqual(Object.keys(legacy.games), games.filter(game => selection[game].includes('normal')));
    }
  }
  await assert.rejects(command('scripts/package-runtime-release.mjs',
    '--output=' + resolve(work, 'empty'), '--games=th06'), error => /at least one Runtime/.test(error.stderr));
  const absent = structuredClone(seed); delete absent.games.th06.runtime; delete absent.games.th06.multiplayerRuntime;
  assert.throws(() => validateSiteManifest(absent), /games/);
  const legacyMpOnly = structuredClone(seed); legacyMpOnly.schema = 'eagler-touhou/site-manifest/1'; delete legacyMpOnly.games.th06.runtime;
  assert.throws(() => validateSiteManifest(legacyMpOnly), /Legacy Site Manifest/);
  console.log('Runtime normal-only, multiplayer-only and mixed collection/Host publication: PASS');
} finally { await rm(work, {recursive: true, force: true}); }
