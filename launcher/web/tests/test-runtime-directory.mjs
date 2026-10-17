/**
 * 检查显式传入的目录 Runtime，不构建游戏、不猜测工作区产物。
 * node tests/test-runtime-directory.mjs --runtime=th08=DIR --runtime=th15=DIR
 * TH10 布局对照可另传 --th10-music-layout=FILE。
 */
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';
import { parseArgs } from 'node:util';
import { runtimeFileNames, runtimeStem } from '../lib/runtime-release.mjs';
import { PRODUCT_GAMES } from '../lib/contracts/product-catalog.mjs';
import { runtimeAppShellPaths } from '../lib/app-shell-policy.mjs';
import { TH10_MUSIC_LAYOUT } from '../lib/th10-content-layout.mjs';

const { values } = parseArgs({
  strict: true,
  options: {
    runtime: { type: 'string', multiple: true },
    'th10-music-layout': { type: 'string' },
  },
});
if (!values.runtime?.length) throw new Error('Pass at least one --runtime=GAME=DIR');
const inputs = new Map();
for (const value of values.runtime) {
  const separator = value.indexOf('=');
  const game = value.slice(0, separator);
  const directory = value.slice(separator + 1);
  if (separator <= 0 || !directory || PRODUCT_GAMES[game]?.runtimeFileLayout !== 'directory') {
    throw new Error(`Invalid directory Runtime input: ${value}`);
  }
  if (inputs.has(game)) throw new Error(`Duplicate Runtime input: ${game}`);
  inputs.set(game, resolve(directory));
}
if (values['th10-music-layout'] && !inputs.has('th10')) {
  throw new Error('--th10-music-layout requires a TH10 Runtime input');
}

const verified = {};
for (const [game, directory] of inputs) {
  const manifest = JSON.parse(await readFile(resolve(directory, 'runtime-files.json'), 'utf8'));
  assert.equal(manifest.schema, 'eagler-touhou/runtime-directory/1');
  const names = runtimeFileNames(game, manifest.files);
  for (const name of names) {
    const bytes = await readFile(resolve(directory, name));
    assert.equal(bytes.length, manifest.files[name].bytes, `${game}/${name}: byte length changed`);
    assert.equal(
      createHash('sha256').update(bytes).digest('hex'),
      manifest.files[name].sha256,
      `${game}/${name}: SHA-256 changed`,
    );
  }
  for (const path of [
    '../outside.js',
    'runtime/../../outside.js',
    `data/${game}.exe`,
    'native/any.dll',
    '/runtime/a.js',
    'runtime/any.data',
  ]) {
    assert.throws(() =>
      runtimeFileNames(game, { ...manifest.files, [path]: { bytes: 1, sha256: '0'.repeat(64) } }),
    );
  }
  assert.doesNotThrow(() =>
    runtimeFileNames(game, {
      ...manifest.files,
      'adapter-worker.wasm': { bytes: 1, sha256: '0'.repeat(64) },
      'adapter-metadata.json': { bytes: 1, sha256: '0'.repeat(64) },
      'fonts/runtime-extra.woff2': { bytes: 1, sha256: '0'.repeat(64) },
    }),
  );
  const requiredWasm = PRODUCT_GAMES[game].runtimeAssets.find((name) => name.endsWith('.wasm'));
  assert.ok(requiredWasm, `${game}: a Wasm program must be declared`);
  const incomplete = { ...manifest.files };
  delete incomplete[requiredWasm];
  assert.throws(() => runtimeFileNames(game, incomplete));
  const stem = runtimeStem(game);
  const paths = runtimeAppShellPaths({
    games: {
      [game]: { runtime: `runtime/${game}/${stem}.html?hosted=1&v=test` },
    },
  });
  for (const path of PRODUCT_GAMES[game].runtimeAssets)
    assert(paths.includes(`runtime/${game}/${path}`));
  verified[game] = { directory, files: names.length };
}
if (values['th10-music-layout']) {
  const canonical = JSON.parse(await readFile(resolve(values['th10-music-layout']), 'utf8'));
  assert.deepEqual(
    canonical,
    TH10_MUSIC_LAYOUT.map((row) => ({ ...row })),
    'Launcher TH10 content layout must match the explicitly selected Runtime layout',
  );
}
console.log(
  JSON.stringify({
    directoryRuntime: 'PASS',
    verified,
    th10MusicLayout: Boolean(values['th10-music-layout']),
  }),
);
