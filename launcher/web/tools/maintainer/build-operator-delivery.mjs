#!/usr/bin/env node
import { archiveDirectory } from '../../lib/archive-directory.mjs';
import { buildNetplayBundle } from '../../../../services/netplay/bundle.mjs';
import { PROJECT_ROOT } from '../../lib/source-layout.mjs';
import { spawn } from 'node:child_process';
import { createHash, randomUUID } from 'node:crypto';
import { createReadStream, existsSync } from 'node:fs';
import { cp, mkdir, readFile, rename, rm, writeFile } from 'node:fs/promises';
import { basename, dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { verifyReleaseBundle } from '../../lib/release-bundle-verifier.mjs';
import { verifyReleaseManifest } from '../../lib/release-manifest.mjs';
import {
  assertHostedSiteMatchesOperatorProfile,
  validateOperatorProfile,
  verifyOperatorDelivery,
  writeOperatorIntegrity,
} from './operator-delivery.mjs';

const project = resolve(fileURLToPath(new URL('../..', import.meta.url)));
const assets = resolve(project, 'tools/maintainer/operator-delivery');

function parseOptions(argv) {
  const result = Object.create(null);
  for (const argument of argv) {
    const index = argument.indexOf('=');
    if (!argument.startsWith('--') || index < 3) throw new Error(`invalid argument: ${argument}`);
    const name = argument.slice(2, index);
    if (Object.hasOwn(result, name) || !argument.slice(index + 1)) throw new Error('duplicate or empty argument: ' + argument);
    result[name] = argument.slice(index + 1);
  }
  const allowed = new Set(['release', 'output', 'archive', 'profile']);
  if (!result.release || !result.output || !result.profile || Object.keys(result).some((key) => !allowed.has(key))) {
    throw new Error(
      'usage: node tools/maintainer/build-operator-delivery.mjs ' +
        '--release=FORMAL_RELEASE --output=NEW_DIRECTORY --profile=FILE [--archive=NEW_ZIP]',
    );
  }
  return result;
}

function run(command, args, cwd = project, stdio = 'inherit') {
  return new Promise((resolvePromise, reject) => {
    const child = spawn(command, args, { cwd, stdio, shell: false });
    child.once('error', reject);
    child.once('exit', (code, signal) =>
      code === 0 ? resolvePromise() : reject(new Error(`${command} failed (${signal || code})`)),
    );
  });
}

async function copyFile(source, target) {
  await mkdir(dirname(target), { recursive: true });
  await cp(source, target);
}

async function sha256File(path) {
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(path)) hash.update(chunk);
  return hash.digest('hex');
}

const options = parseOptions(process.argv.slice(2));
const formalRelease = resolve(options.release);
const output = resolve(options.output);
const archive = options.archive ? resolve(options.archive) : null;
const archiveChecksum = archive ? `${archive}.sha256` : null;
const profilePath = resolve(options.profile);
if (existsSync(output)) throw new Error(`Operator output must not already exist: ${output}`);
if (archive && (existsSync(archive) || existsSync(archiveChecksum))) {
  throw new Error(`Operator archive or checksum already exists: ${archive}`);
}

const formalManifest = await verifyReleaseBundle(formalRelease);
const hosted = resolve(formalRelease, 'hosted-site');
const hostedRelease = await verifyReleaseManifest(hosted);
const profile = validateOperatorProfile(JSON.parse(await readFile(profilePath, 'utf8')));
const hostManifest = JSON.parse(await readFile(resolve(hosted, 'site-manifest.json'), 'utf8'));
assertHostedSiteMatchesOperatorProfile(hostManifest, profile);

const incompleteRoot = resolve(dirname(output), '.operator-incomplete');
const operationRoot = resolve(incompleteRoot, randomUUID());
const candidate = resolve(operationRoot, basename(output));
await mkdir(candidate, { recursive: true });
try {
  await cp(resolve(assets, 'payload'), candidate, { recursive: true });
  await cp(hosted, resolve(candidate, 'site'), { recursive: true });
  await copyFile(profilePath, resolve(candidate, 'operator-profile.json'));
  await copyFile(
    resolve(formalRelease, 'verification-report.json'),
    resolve(candidate, 'verification-report.json'),
  );

  await copyFile(
    resolve(assets, 'nginx.migration-window.conf.example'),
    resolve(candidate, 'services/nginx/nginx.migration-window.conf.example'),
  );
  await copyFile(
    resolve(assets, 'nginx.final-https.conf.example'),
    resolve(candidate, 'services/nginx/nginx.final-https.conf.example'),
  );
  await copyFile(
    resolve(assets, 'relay.env.example'),
    resolve(candidate, 'services/relay/relay.env.example'),
  );
  await copyFile(
    resolve(assets, 'eagler-netplay-relay.service'),
    resolve(candidate, 'services/relay/eagler-netplay-relay.service'),
  );

  const mappings = [
    ['scripts/verify-origin-cutover.mjs', 'services/verify/verify-origin-cutover.mjs'],
    ['scripts/verify-hsts-cutover.mjs', 'services/verify/verify-hsts-cutover.mjs'],
    [
      'tools/maintainer/verify-public-relay-fallback.mjs',
      'services/verify/verify-public-relay-fallback.mjs',
    ],
    [
      'tools/maintainer/verify-public-targeted-relay.mjs',
      'services/verify/verify-public-targeted-relay.mjs',
    ],
  ];
  for (const [source, target] of mappings)
    await copyFile(resolve(project, source), resolve(candidate, target));
  const serviceRoot = resolve(PROJECT_ROOT, 'services/netplay');
  await buildNetplayBundle(resolve(candidate, 'services/relay'));
  await copyFile(
    resolve(serviceRoot, 'render-coturn-config.cjs'),
    resolve(candidate, 'services/turn/render-coturn-config.cjs'),
  );
  await copyFile(
    resolve(serviceRoot, 'coturn.env.example'),
    resolve(candidate, 'services/turn/turn.env.example'),
  );

  const operatorManifest = await writeOperatorIntegrity(candidate, {
    formalReleaseId: formalManifest.releaseId,
    siteReleaseId: hostedRelease.releaseId,
    profile,
  });
  await verifyOperatorDelivery(candidate);
  await mkdir(dirname(output), { recursive: true });
  await rename(candidate, output);
  console.log(`Operator delivery: ${output}`);
  console.log(`Operator identity: ${operatorManifest.operatorId}`);
} finally {
  await rm(operationRoot, { recursive: true, force: true });
  try {
    await rm(incompleteRoot);
  } catch (error) {
    if (!['ENOENT', 'ENOTEMPTY', 'EPERM'].includes(error?.code)) throw error;
  }
}

if (archive) {
  const temporaryArchive = `${archive}.incomplete-${randomUUID()}`;
  const temporaryChecksum = `${archiveChecksum}.incomplete-${randomUUID()}`;
  try {
    await mkdir(dirname(archive), { recursive: true });
    await archiveDirectory(output, temporaryArchive, { prefix: basename(output) });
    const digest = await sha256File(temporaryArchive);
    await writeFile(temporaryChecksum, `${digest}  ${basename(archive)}\n`);
    await rename(temporaryArchive, archive);
    await rename(temporaryChecksum, archiveChecksum);
    console.log(`Operator archive: ${archive}`);
    console.log(`Operator archive SHA-256: ${digest}`);
  } finally {
    await rm(temporaryArchive, { force: true });
    await rm(temporaryChecksum, { force: true });
  }
}
