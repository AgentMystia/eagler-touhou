#!/usr/bin/env node
import { existsSync } from 'node:fs';
import { cp, mkdir, mkdtemp, readFile, rename, rm } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { verifyRuntimeRelease } from '../lib/runtime-release.mjs';
import { archiveDirectory } from '../lib/archive-directory.mjs';
import { run } from '../host/lib/process.mjs';

const project = fileURLToPath(new URL('../', import.meta.url));
export async function buildSelfHostBundle({
  runtimeRelease,
  output,
  unicodeFont,
  fontLicense,
  archive,
}) {
  if (!runtimeRelease || !output || !unicodeFont)
    throw new Error('runtime-release, output and unicode-font are required');
  const destination = resolve(output);
  if (existsSync(destination)) throw new Error('Self-host output already exists: ' + destination);
  if (archive && existsSync(resolve(archive)))
    throw new Error('Archive already exists: ' + archive);
  const font = resolve(unicodeFont),
    license = resolve(fontLicense ?? resolve(dirname(font), 'LICENSE.txt'));
  for (const file of [font, license])
    if (!existsSync(file)) throw new Error('Font input missing: ' + file);
  const manifest = await verifyRuntimeRelease(resolve(runtimeRelease));
  await run(process.execPath, [resolve(project, 'scripts/build-launcher.mjs')], { cwd: project });
  const { SELF_HOST_BUNDLE_COPY_RULES } = await import('../lib/self-host-bundle.mjs');
  await mkdir(dirname(destination), { recursive: true });
  const stage = await mkdtemp(resolve(dirname(destination), '.self-host-stage-'));
  try {
    for (const rule of SELF_HOST_BUNDLE_COPY_RULES) {
      const target = resolve(stage, rule.target);
      await mkdir(dirname(target), { recursive: true });
      await cp(resolve(project, rule.source), target);
    }
    for (const game of Object.keys(manifest.games))
      await mkdir(resolve(stage, 'games', game), { recursive: true });
    await mkdir(resolve(stage, 'shared'));
    await cp(resolve(runtimeRelease), resolve(stage, 'runtime-release'), { recursive: true });
    await cp(font, resolve(stage, 'shared/unifont.otf'));
    await cp(license, resolve(stage, 'shared/unifont.LICENSE.txt'));
    await run(
      process.execPath,
      [
        resolve(project, 'scripts/write-self-host-provenance.mjs'),
        '--output=' + resolve(stage, 'self-host-provenance.json'),
      ],
      { cwd: project },
    );
    await rename(stage, destination);
  } finally {
    await rm(stage, { recursive: true, force: true });
  }
  if (archive) await archiveDirectory(destination, archive);
  return destination;
}
if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  const options = {};
  for (const value of process.argv.slice(2)) {
    const match = /^--(runtime-release|output|unicode-font|font-license|archive)=(.+)$/.exec(value);
    if (!match || Object.hasOwn(options, match[1]))
      throw new Error('Invalid or duplicate argument: ' + value);
    options[match[1]] = match[2];
  }
  const output = await buildSelfHostBundle({
    runtimeRelease: options['runtime-release'],
    output: options.output,
    unicodeFont: options['unicode-font'],
    fontLicense: options['font-license'],
    archive: options.archive,
  });
  console.log('Self-host bundle: ' + output);
}
