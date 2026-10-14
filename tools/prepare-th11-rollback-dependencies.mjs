/** Prepare only this isolated TH11 Launcher worktree from existing local packages.
 * No install, network, build, test, source-tree writes or unknown-file replacement.
 */
import {createHash} from 'node:crypto';
import {lstat, mkdir, readFile, readdir, realpath, stat, symlink, writeFile} from 'node:fs/promises';
import {createRequire} from 'node:module';
import {basename, isAbsolute, relative, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const targetRoot = await realpath(fileURLToPath(new URL('../', import.meta.url)));
const args = process.argv.slice(2);
if (args.length !== 2 || args[0] !== '--source-root')
  throw Error('Usage: node tools/prepare-th11-rollback-dependencies.mjs --source-root <existing Launcher root>');
const sourceRoot = await realpath(resolve(args[1]));
if (basename(targetRoot) !== 'launcher-th11-rollback' || targetRoot === sourceRoot)
  throw Error('This preparation is confined to the isolated launcher-th11-rollback worktree');
const sourceModules = await realpath(resolve(sourceRoot, 'node_modules'));
const targetModules = resolve(targetRoot, 'node_modules');
const insideSource = relative(sourceRoot, targetRoot);
if (!isAbsolute(insideSource) && insideSource !== '..' && !insideSource.startsWith('..' + (process.platform === 'win32' ? '\\' : '/')))
  throw Error('Target must not be inside the source dependency tree');
const digest = bytes => createHash('sha256').update(bytes).digest('hex');
const [targetLock, sourceLock] = await Promise.all([
  readFile(resolve(targetRoot, 'package-lock.json')), readFile(resolve(sourceRoot, 'package-lock.json')),
]);
if (!targetLock.equals(sourceLock))
  throw Error('Source and companion package-lock.json differ; review dependency identity before preparing');
const workspaceMap = new Map([
  ['@eagler/contracts', 'contracts'],
  ['@eagler/web', 'launcher/web'],
  ['@eagler/netplay', 'services/netplay'],
]);
const rootManifest = JSON.parse(await readFile(resolve(targetRoot, 'package.json'), 'utf8'));
if (JSON.stringify([...rootManifest.workspaces].sort()) !== JSON.stringify([...workspaceMap.values()].sort()))
  throw Error('Workspace list changed; explicit isolated mappings must be reviewed');
for (const [name, directory] of workspaceMap) {
  const manifest = JSON.parse(await readFile(resolve(targetRoot, directory, 'package.json'), 'utf8'));
  if (manifest.name !== name) throw Error('Unexpected workspace identity: ' + directory);
}
const plan = [];
for (const entry of await readdir(sourceModules, {withFileTypes: true})) {
  if (entry.name === '@eagler' || (entry.name.startsWith('.') && entry.name !== '.bin')) continue;
  const source = resolve(sourceModules, entry.name);
  if (!(await stat(source)).isDirectory()) throw Error('Unexpected top-level dependency file: ' + source);
  plan.push({name: entry.name, destination: resolve(targetModules, entry.name), source: await realpath(source)});
}
for (const [name, directory] of workspaceMap)
  plan.push({name, destination: resolve(targetModules, name), source: await realpath(resolve(targetRoot, directory))});
async function optionalStat(path) {
  try { return await lstat(path); } catch (error) { if (error.code === 'ENOENT') return null; throw error; }
}
async function ownDirectory(path) {
  const info = await optionalStat(path);
  if (info && (!info.isDirectory() || info.isSymbolicLink()))
    throw Error('Refusing non-owned directory: ' + path);
}
await ownDirectory(targetModules);
await ownDirectory(resolve(targetModules, '@eagler'));
const markerPath = resolve(targetModules, '.th11-rollback-dependencies.json');
const markerInfo = await optionalStat(markerPath);
if (markerInfo) {
  if (!markerInfo.isFile() || markerInfo.isSymbolicLink()) throw Error('Refusing unknown preparation marker');
  const old = JSON.parse(await readFile(markerPath, 'utf8'));
  if (old.schema !== 1 || old.targetRoot !== targetRoot || old.sourceRoot !== sourceRoot ||
      old.packageLockSha256 !== digest(targetLock)) throw Error('Existing preparation marker has another identity');
}
if (await optionalStat(targetModules)) {
  const expected = new Set(plan.filter(item => !item.name.startsWith('@eagler/')).map(item => item.name));
  expected.add('@eagler'); expected.add('.th11-rollback-dependencies.json');
  for (const name of await readdir(targetModules))
    if (!expected.has(name)) throw Error('Refusing unknown existing dependency entry: ' + name);
}
if (await optionalStat(resolve(targetModules, '@eagler'))) {
  const expected = new Set([...workspaceMap.keys()].map(name => name.split('/')[1]));
  for (const name of await readdir(resolve(targetModules, '@eagler')))
    if (!expected.has(name)) throw Error('Refusing unknown @eagler workspace: ' + name);
}
// Validate the entire plan before the first write. An interruption can safely be
// resumed: only an existing symlink/junction to the exact planned identity is accepted.
for (const item of plan) {
  const info = await optionalStat(item.destination);
  if (info && (!info.isSymbolicLink() || await realpath(item.destination) !== item.source))
    throw Error('Refusing existing dependency path: ' + item.destination);
}
await mkdir(resolve(targetModules, '@eagler'), {recursive: true});
let created = 0;
for (const item of plan) {
  if (await optionalStat(item.destination)) continue;
  await symlink(item.source, item.destination, process.platform === 'win32' ? 'junction' : 'dir');
  created++;
}
const identities = [];
for (const project of ['launcher/web', 'services/netplay']) {
  const require = createRequire(resolve(targetRoot, project, 'package.json'));
  const resolved = require.resolve('@eagler/contracts/product-catalog');
  const actual = await realpath(resolved);
  const expected = await realpath(resolve(targetRoot, 'contracts/src/product-catalog.mts'));
  if (actual !== expected) throw Error('Contracts escaped isolated worktree from ' + project + ': ' + actual);
  identities.push({project, specifier: '@eagler/contracts/product-catalog', resolved, realpath: actual});
  for (const specifier of project === 'launcher/web'
    ? ['typescript/package.json', 'esbuild/package.json', 'react/package.json', 'react-dom/package.json']
    : ['ws/package.json']) {
    const resolved = require.resolve(specifier);
    identities.push({project, specifier, resolved, realpath: await realpath(resolved)});
  }
}
for (const [name, directory] of workspaceMap)
  if (await realpath(resolve(targetModules, name)) !== await realpath(resolve(targetRoot, directory)))
    throw Error('Workspace junction verification failed: ' + name);
const receipt = {schema: 1, preparedAt: new Date().toISOString(), targetRoot, sourceRoot,
  packageLockSha256: digest(targetLock), scriptSha256: digest(await readFile(fileURLToPath(import.meta.url))),
  created, reused: plan.length - created, identities};
await writeFile(markerPath, JSON.stringify(receipt, null, 2) + '\n');
console.log(JSON.stringify(receipt, null, 2));

