import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';

/** Authored files define the cascade; comments and selectors are ordinary CSS. */
export function launcherStyleSources(shell, fonts, features) {
  return Object.freeze({shell: fonts + '\n' + shell, features});
}
export async function readLauncherStyleSources(project) {
  const [shell, fonts, features] = await Promise.all(
    ['styles.css', 'ui-fonts.css', 'features.css'].map(file => readFile(resolve(project, 'public', file), 'utf8')));
  return launcherStyleSources(shell, fonts, features);
}
export async function buildLauncherStyleOutputs({project, outputDirectory}) {
  const {build} = await import('esbuild');
  const sources = await readLauncherStyleSources(project);
  const outputs = [];
  for (const [key, filename] of [['shell', 'styles.css'], ['features', 'features.css']]) {
    const result = await build({
      stdin: {contents: sources[key], resolveDir: resolve(project, 'public'), sourcefile: filename, loader: 'css'},
      outfile: resolve(outputDirectory, filename), bundle: true, minify: true, write: false,
      external: ['*.woff2', '*.webp', '*.png', '*.svg', '*.jpg'], logLevel: 'warning',
    });
    outputs.push(...result.outputFiles);
  }
  const touch = await build({
    entryPoints: [resolve(project, 'public/touch-guide.css')], outfile: resolve(outputDirectory, 'touch-guide.css'),
    bundle: true, minify: true, write: false, external: ['*.woff2', '*.webp', '*.png', '*.svg', '*.jpg'], logLevel: 'warning',
  });
  return [...outputs, ...touch.outputFiles];
}

/** The unchanged classic-worker contract can be compiled independently of
 * the React UI graph. Self-host distributions still ship its compiled artifact. */
export async function buildRuntimeGenerationWorkerSource({ project }) {
  const {contractsSourceDirectory} = await import('./source-layout.mjs');
  const { build } = await import("esbuild");
  const source = contractsSourceDirectory(project);
  if (!source) throw new Error('Runtime worker compilation needs the contracts source package');
  const result = await build({absWorkingDir: project, entryPoints: [resolve(source, "runtime-generations.mts")],
    bundle: true, format: "iife", globalName: "EaglerRuntimeGenerations", target: "es2022", logLevel: "warning", write: false});
  return result.outputFiles[0].text;
}
