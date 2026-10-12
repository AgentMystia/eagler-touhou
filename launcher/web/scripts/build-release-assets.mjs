#!/usr/bin/env node
import { createHash } from 'node:crypto';
import { createReadStream, existsSync } from 'node:fs';
import { mkdir, mkdtemp, readFile, rename, rm, writeFile } from 'node:fs/promises';
import { basename, dirname, resolve } from 'node:path';
import { verifyReleaseBundle } from '../lib/release-bundle-verifier.mjs';
import { verifyRuntimeRelease } from '../lib/runtime-release.mjs';
import { archiveDirectory } from '../lib/archive-directory.mjs';
import { buildSelfHostBundle } from './build-self-host-bundle.mjs';

const options = {};
for (const value of process.argv.slice(2)) {
  const match = /^--(candidate|output|unicode-font|font-license)=(.+)$/.exec(value);
  if (!match || Object.hasOwn(options, match[1]))
    throw new Error('Invalid or duplicate argument: ' + value);
  options[match[1]] = match[2];
}
for (const name of ['candidate', 'output', 'unicode-font'])
  if (!options[name]) throw new Error('Missing --' + name);
const candidate = resolve(options.candidate),
  output = resolve(options.output);
if (existsSync(output)) throw new Error('Release assets already exist: ' + output);
const release = await verifyReleaseBundle(candidate);
const runtimeRelease = resolve(candidate, 'runtime-release'),
  runtime = await verifyRuntimeRelease(runtimeRelease);
await mkdir(dirname(output), { recursive: true });
const stage = await mkdtemp(resolve(dirname(output), '.release-assets-stage-'));
try {
  const bundle = resolve(stage, '.self-host');
  await buildSelfHostBundle({
    runtimeRelease,
    output: bundle,
    unicodeFont: options['unicode-font'],
    fontLicense: options['font-license'],
    archive: resolve(stage, 'EaglerTouhou-SelfHost.zip'),
  });
  await rm(bundle, { recursive: true });
  await archiveDirectory(runtimeRelease, resolve(stage, 'EaglerTouhou-Runtime.zip'), {
    prefix: 'runtime-release',
  });
  const checksums = [];
  for (const name of ['EaglerTouhou-SelfHost.zip', 'EaglerTouhou-Runtime.zip']) {
    const hash = createHash('sha256');
    for await (const bytes of createReadStream(resolve(stage, name))) hash.update(bytes);
    checksums.push(hash.digest('hex') + '  ' + name);
  }
  await writeFile(resolve(stage, 'SHA256SUMS.txt'), checksums.join('\n') + '\n');
  await writeFile(
    resolve(stage, 'release-assets.json'),
    JSON.stringify(
      {
        schema: 'eagler-touhou/github-release-assets/1',
        sourceReleaseId: release.releaseId,
        runtimeReleaseSchema: runtime.schema,
        runtimeGames: Object.keys(runtime.games),
        recommendedAsset: 'EaglerTouhou-SelfHost.zip',
        advancedAsset: 'EaglerTouhou-Runtime.zip',
      },
      null,
      2,
    ) + '\n',
  );
  await rename(stage, output);
} finally {
  await rm(stage, { recursive: true, force: true });
}
console.log('Release assets: ' + output);
