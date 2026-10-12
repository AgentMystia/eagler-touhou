import test from 'node:test';
import assert from 'node:assert/strict';
import { access } from 'node:fs/promises';
import { basename, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import postcss from 'postcss';
import { buildLauncherStyleOutputs, launcherStyleSources } from '../../lib/frontend-assets.mjs';

const project = fileURLToPath(new URL('../..', import.meta.url));
test('shell and features compile to valid standalone styles with available local assets', async () => {
  const outputs = await buildLauncherStyleOutputs({
    project,
    outputDirectory: resolve(project, '.cache/style-check'),
  });
  assert.deepEqual(outputs.map((file) => basename(file.path)).sort(), [
    'features.css',
    'styles.css',
    'touch-guide.css',
  ]);
  for (const output of outputs) {
    const css = postcss.parse(output.text);
    assert.ok(css.nodes.some((node) => node.type === 'rule'));
    const resources = [];
    css.walkDecls((declaration) => {
      for (const match of declaration.value.matchAll(/url\((?:["']?)([^)"']+)(?:["']?)\)/g)) {
        if (!/^(?:data:|https?:|#)/.test(match[1])) resources.push(match[1].split(/[?#]/)[0]);
      }
    });
    await Promise.all(resources.map((path) => access(resolve(project, 'public', path))));
  }
});
test('style composition preserves authored order without interpreting comments or selectors', () => {
  const shell = '/* 任意说明 */\nbody { color: red; }';
  const features = '.dialog { color: blue; }';
  const result = launcherStyleSources(shell, '@font-face {font-family: example}', features);
  assert.ok(result.shell.endsWith(shell));
  assert.equal(result.features, features);
});
