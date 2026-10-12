import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve } from 'node:path';
import { publishRuntimeDirectory, verifyRuntimeDirectory } from '../runtime-package.mjs';

async function fixture(t) {
  const root = await mkdtemp(resolve(tmpdir(), 'runtime-publish-'));
  t.after(() => rm(root, { recursive: true, force: true }));
  return resolve(root, 'runtime');
}
async function produce(directory, value, game = 'th09') {
  await mkdir(directory, { recursive: true });
  const sources = { 'manifest.json': JSON.stringify({ game }), 'runtime.mjs': value };
  const files = {};
  for (const [name, value] of Object.entries(sources)) {
    const bytes = Buffer.from(value); await writeFile(resolve(directory, name), bytes);
    files[name] = { bytes: bytes.length, sha256: createHash('sha256').update(bytes).digest('hex') };
  }
  await writeFile(resolve(directory, 'runtime-files.json'), JSON.stringify({ schema: 'eagler-touhou/runtime-directory/1', files }));
}
test('verified replacements publish the whole generation', async t => {
  const output = await fixture(t);
  await publishRuntimeDirectory({ output, game: 'th09', produce: path => produce(path, 'old') });
  await publishRuntimeDirectory({ output, game: 'th09', produce: path => produce(path, 'new') });
  assert.equal(await readFile(resolve(output, 'runtime.mjs'), 'utf8'), 'new');
  await verifyRuntimeDirectory(output, 'th09');
});
test('producer failure and wrong game preserve the previous generation', async t => {
  const output = await fixture(t); await produce(output, 'old');
  await assert.rejects(publishRuntimeDirectory({ output, game: 'th09', produce: async path => { await writeFile(resolve(path, 'partial'), 'x'); throw new Error('compile failed'); } }), /compile failed/);
  await assert.rejects(publishRuntimeDirectory({ output, game: 'th09', produce: path => produce(path, 'wrong', 'th10') }), /游戏身份/);
  assert.equal(await readFile(resolve(output, 'runtime.mjs'), 'utf8'), 'old');
});
test('unmanaged output is preserved without invoking the producer', async t => {
  const output = await fixture(t); await mkdir(output); await writeFile(resolve(output, 'notes.txt'), 'keep');
  let called = false;
  await assert.rejects(publishRuntimeDirectory({ output, game: 'th09', produce: () => { called = true; } }));
  assert.equal(called, false);
  assert.equal(await readFile(resolve(output, 'notes.txt'), 'utf8'), 'keep');
});
test('tampering, unlisted files and traversal are rejected', async t => {
  const output = await fixture(t); await produce(output, 'valid');
  await writeFile(resolve(output, 'runtime.mjs'), 'tampered');
  await assert.rejects(verifyRuntimeDirectory(output, 'th09'), /文件身份/);
  await produce(output, 'valid'); await writeFile(resolve(output, 'extra'), 'x');
  await assert.rejects(verifyRuntimeDirectory(output, 'th09'), /文件集合/);
  await rm(resolve(output, 'extra'));
  const manifest = JSON.parse(await readFile(resolve(output, 'runtime-files.json'), 'utf8'));
  manifest.files['../outside'] = manifest.files['runtime.mjs'];
  await writeFile(resolve(output, 'runtime-files.json'), JSON.stringify(manifest));
  await assert.rejects(verifyRuntimeDirectory(output, 'th09'), /路径/);
});
