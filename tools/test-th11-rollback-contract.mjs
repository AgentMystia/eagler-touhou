/** Serial TH11 Launcher contract acceptance; does not build or install.
 * Usage: node tools/test-th11-rollback-contract.mjs --output artifacts/th11-rollback-contract-01
 */
import {createHash} from 'node:crypto';
import {execFile, spawn} from 'node:child_process';
import {mkdir, open, readFile, readdir, realpath, writeFile} from 'node:fs/promises';
import {createRequire} from 'node:module';
import {isAbsolute, relative, resolve, sep} from 'node:path';
import {fileURLToPath} from 'node:url';
import {promisify} from 'node:util';

const execute = promisify(execFile);
const root = await realpath(fileURLToPath(new URL('../', import.meta.url)));
const args = process.argv.slice(2);
if (args.length !== 2 || args[0] !== '--output')
  throw Error('Usage: node tools/test-th11-rollback-contract.mjs --output <fresh artifacts directory>');
const artifacts = resolve(root, 'artifacts');
const output = resolve(root, args[1]);
const relativeOutput = relative(artifacts, output);
if (!relativeOutput || isAbsolute(relativeOutput) || relativeOutput === '..' || relativeOutput.startsWith('..' + sep))
  throw Error('Output must be a fresh descendant of this worktree artifacts directory');
await mkdir(artifacts, {recursive: true});
await mkdir(resolve(output, '..'), {recursive: true});
if (await realpath(resolve(output, '..')) !== resolve(output, '..'))
  throw Error('Output parent must not traverse a symlink/junction');
await mkdir(output); // EEXIST is an intentional refusal; never overwrite an old receipt.
if (await realpath(output) !== output) throw Error('Output must not traverse a symlink/junction');
const digest = bytes => createHash('sha256').update(bytes).digest('hex');
const slash = path => path.split(sep).join('/');
const receipt = {
  schema: 1, kind: 'th11-launcher-rollback-contract', startedAt: new Date().toISOString(),
  root, output, invocation: [process.execPath, ...process.argv.slice(1)],
  runtime: {executable: process.execPath, versions: process.versions, platform: process.platform, arch: process.arch},
  scope: 'Actual contract modules + isolated loopback relay + authored services/React in synthetic DOM. No game/browser/performance evidence.',
  passed: false, sourceStable: false, generatedStable: false, commands: [],
};
const receiptPath = resolve(output, 'receipt.json');
async function save() { await writeFile(receiptPath, JSON.stringify(receipt, null, 2) + '\n'); }
async function git(args) {
  const result = await execute('git', args, {cwd: root, encoding: 'utf8', windowsHide: true, maxBuffer: 8 * 1024 * 1024});
  return {command: ['git', ...args], stdout: result.stdout, stderr: result.stderr};
}
async function repository() {
  const head = await git(['rev-parse', 'HEAD']);
  const branch = await git(['rev-parse', '--abbrev-ref', 'HEAD']);
  const status = await git(['status', '--porcelain=v1', '--untracked-files=all']);
  return {head: head.stdout.trim(), branch: branch.stdout.trim(), status: status.stdout, commands: [head, branch, status]};
}
async function walk(directory, accept, found) {
  for (const entry of (await readdir(resolve(root, directory), {withFileTypes: true})).sort((a,b) => a.name.localeCompare(b.name))) {
    const path = directory + '/' + entry.name;
    if (entry.isSymbolicLink()) throw Error('Source identity cannot follow unreviewed symlink: ' + path);
    if (entry.isDirectory()) await walk(path, accept, found);
    else if (entry.isFile() && accept(path)) found.add(path);
  }
}
async function filesIdentity(paths) {
  const files = [];
  for (const path of [...paths].sort()) {
    const bytes = await readFile(resolve(root, path));
    files.push({path: slash(path), bytes: bytes.length, sha256: digest(bytes)});
  }
  return {sha256: digest(JSON.stringify(files)), files};
}
async function identities() {
  const source = new Set([
    'package.json', 'package-lock.json', 'contracts/package.json', 'contracts/tsconfig.json',
    'launcher/web/package.json', 'launcher/web/tsconfig.launcher.json', 'launcher/web/tsconfig.ui.json',
    'services/netplay/package.json',
    'launcher/web/tests/test-th11-multiplayer-policy.mjs',
    'launcher/web/tests/test-multiplayer-runtime-options.mjs',
    'tools/prepare-th11-rollback-dependencies.mjs', 'tools/test-th11-rollback-contract.mjs',
  ]);
  for (const directory of ['contracts/src', 'launcher/web/src', 'launcher/web/app',
    'launcher/web/lib', 'launcher/web/tests/ui', 'services/netplay'])
    await walk(directory, path => /\.(?:mts|ts|tsx|mjs|json)$/.test(path), source);
  const generated = new Set(['launcher/web/.cache/client-services-build.json']);
  await walk('launcher/web/.cache/build/browser', () => true, generated);
  return {source: await filesIdentity(source), generated: await filesIdentity(generated)};
}
async function currentDependencies() {
  const prepared = JSON.parse(await readFile(resolve(root, 'node_modules/.th11-rollback-dependencies.json'), 'utf8'));
  const expected = await realpath(resolve(root, 'contracts/src/product-catalog.mts'));
  const catalog = [];
  for (const project of ['launcher/web', 'services/netplay']) {
    const require = createRequire(resolve(root, project, 'package.json'));
    const resolved = require.resolve('@eagler/contracts/product-catalog');
    const actual = await realpath(resolved);
    if (actual !== expected) throw Error('Wrong contracts identity from ' + project + ': ' + actual);
    catalog.push({project, resolved, realpath: actual});
  }
  if (prepared.targetRoot !== root) throw Error('Dependency receipt belongs to another tree');
  return {preparation: prepared, liveCatalogResolution: catalog,
    preparationReceiptSha256: digest(await readFile(resolve(root, 'node_modules/.th11-rollback-dependencies.json')))};
}
const tests = [
  {id: '01-th11-policy-relay', args: ['launcher/web/tests/test-th11-multiplayer-policy.mjs']},
  {id: '02-runtime-options-regression', args: ['launcher/web/tests/test-multiplayer-runtime-options.mjs']},
  {id: '03-th11-room-ui', args: ['--test', '--test-concurrency=1', '--test-name-pattern=TH11',
    'launcher/web/tests/ui/multiplayer-services.test.mjs', 'launcher/web/tests/ui/room-views.test.mjs']},
];
async function run(spec) {
  const result = {id: spec.id, command: [process.execPath, ...spec.args], cwd: root,
    startedAt: new Date().toISOString(), stdout: spec.id + '.stdout.log',
    stderr: spec.id + '.stderr.log', header: spec.id + '.header.json', exitCode: null, signal: null};
  const stdout = await open(resolve(output, result.stdout), 'wx');
  const stderr = await open(resolve(output, result.stderr), 'wx');
  await writeFile(resolve(output, result.header), JSON.stringify(result, null, 2) + '\n', {flag: 'wx'});
  receipt.commands.push(result);
  await save();
  const started = performance.now();
  console.log('[TH11 contract] starting ' + spec.id);
  try {
    await new Promise((accept) => {
      const child = spawn(process.execPath, spec.args, {cwd: root, windowsHide: true, shell: false,
        stdio: ['ignore', stdout.fd, stderr.fd]});
      child.once('error', error => { result.spawnError = String(error.stack || error); });
      child.once('close', (code, signal) => { result.exitCode = code; result.signal = signal; accept(); });
    });
  } finally {
    await stdout.close(); await stderr.close();
    result.endedAt = new Date().toISOString(); result.elapsedMs = performance.now() - started;
    for (const stream of ['stdout', 'stderr']) {
      const bytes = await readFile(resolve(output, result[stream]));
      result[stream + 'Bytes'] = bytes.length; result[stream + 'Sha256'] = digest(bytes);
    }
    result.passed = result.exitCode === 0 && !result.signal && !result.spawnError;
    await writeFile(resolve(output, result.header), JSON.stringify(result, null, 2) + '\n');
    await save();
  }
  console.log('[TH11 contract] ' + spec.id + ' exit=' + result.exitCode + ' passed=' + result.passed);
  if (!result.passed) {
    for (const stream of ['stdout', 'stderr']) {
      const tail = (await readFile(resolve(output, result[stream]), 'utf8')).slice(-6000);
      if (tail) console.error('[' + spec.id + ' ' + stream + ' tail]\n' + tail);
    }
  }
  return result.passed;
}
try {
  await save();
  receipt.repositoryBefore = await repository();
  receipt.dependencies = await currentDependencies();
  receipt.before = await identities();
  await save();
  for (const test of tests) {
    if (!await run(test)) {
      receipt.stoppedAfter = test.id;
      break;
    }
  }
} catch (error) {
  receipt.error = String(error.stack || error);
  console.error(receipt.error);
} finally {
  try {
    receipt.after = await identities();
    receipt.repositoryAfter = await repository();
    receipt.sourceStable = !!receipt.before && receipt.before.source.sha256 === receipt.after.source.sha256 &&
      receipt.repositoryBefore.head === receipt.repositoryAfter.head;
    receipt.generatedStable = !!receipt.before && receipt.before.generated.sha256 === receipt.after.generated.sha256;
    receipt.dependenciesAfter = await currentDependencies();
    receipt.dependenciesStable = JSON.stringify(receipt.dependencies) === JSON.stringify(receipt.dependenciesAfter);
  } catch (error) { receipt.finalIdentityError = String(error.stack || error); }
  receipt.notRun = tests.filter(test => !receipt.commands.some(run => run.id === test.id)).map(test => test.id);
  receipt.endedAt = new Date().toISOString();
  receipt.passed = !receipt.error && !receipt.finalIdentityError && receipt.sourceStable &&
    receipt.generatedStable && receipt.dependenciesStable && receipt.commands.length === tests.length &&
    receipt.commands.every(command => command.passed);
  await save();
  console.log('[TH11 contract] receipt=' + receiptPath + ' passed=' + receipt.passed);
  if (!receipt.passed) process.exitCode = 1;
}

