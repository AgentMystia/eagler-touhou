import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, rm } from 'node:fs/promises';
import { resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { build } from 'esbuild';
import { authoredSourcesPlugin } from './authored-sources.mjs';
const project = fileURLToPath(new URL('../..', import.meta.url));
test('site notice parsing, persistence, lifecycle and mounted gestures', async () => {
  await mkdir(resolve(project, '.cache'), { recursive: true });
  const directory = await mkdtemp(resolve(project, '.cache/site-notice-'));
  try {
    const output = resolve(directory, 'checks.mjs');
    await build({
      entryPoints: [resolve(project, 'tests/ui/site-notice.checks.tsx')],
      outfile: output,
      bundle: true,
      platform: 'node',
      format: 'esm',
      jsx: 'automatic',
      packages: 'external',
      plugins: [authoredSourcesPlugin(project)],
      logLevel: 'silent',
    });
    const { runChecks } = await import(pathToFileURL(output).href);
    const result = await runChecks();
    assert.equal(result.status, 'passed');
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});
