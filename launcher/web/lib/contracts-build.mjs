import {existsSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {mkdir, mkdtemp, readFile, readdir, rename, rm, writeFile} from 'node:fs/promises';
import {relative, resolve} from 'node:path';
import {spawn} from 'node:child_process';
import {contractsSourceDirectory, dependencyPath, dependencyExecutable, WEB_ROOT} from './source-layout.mjs';

const builds = new Map();
const workerName = 'runtime-generations-worker.js';
const digest = value => createHash('sha256').update(value).digest('hex');

async function sources(directory) {
  const files = [];
  for (const entry of await readdir(directory, {withFileTypes: true})) {
    const path = resolve(directory, entry.name);
    if (entry.isDirectory()) files.push(...await sources(path));
    else if (entry.isFile() && entry.name.endsWith('.mts')) files.push(path);
  }
  return files.sort();
}

async function valid(root, inputHash) {
  try {
    const manifest = JSON.parse(await readFile(resolve(root, 'manifest.json'), 'utf8'));
    if (manifest.inputHash !== inputHash) return false;
    for (const [path, hash] of Object.entries(manifest.outputs)) {
      if (digest(await readFile(resolve(root, path))) !== hash) return false;
    }
    return true;
  } catch (error) {
    if (error.code === 'ENOENT' || error instanceof SyntaxError) return false;
    throw error;
  }
}

async function prepare(project, attempt = 0) {
  const source = contractsSourceDirectory(project);
  if (!source) return {mode: 'prebuilt', root: project, contractsDirectory: resolve(project, 'assets/contracts'), inputHash: null};
  const files = await sources(source);
  const config = resolve(source, '../tsconfig.json');
  const inputs = [...files, config, dependencyPath('typescript/package.json', project),
    dependencyPath('esbuild/package.json', project), ...['frontend-assets.mjs', 'contracts-build.mjs', 'source-layout.mjs'].map(name => resolve(project, 'lib', name))];
  const fingerprint = paths => Promise.all(paths.map(async path => [relative(source, path).replaceAll('\\', '/'), digest(await readFile(path))]));
  const entries = await fingerprint(inputs);
  const inputHash = digest(JSON.stringify(entries));
  const cache = resolve(project, '.cache/build/contracts');
  const root = resolve(cache, inputHash);
  const result = {mode: 'source', root, contractsDirectory: resolve(root, 'assets/contracts'), inputHash};
  if (await valid(root, inputHash)) return result;
  if (existsSync(root)) throw new Error('Contracts build cache is corrupt: ' + root);
  await mkdir(cache, {recursive: true});
  const staging = await mkdtemp(resolve(cache, '.staging-'));
  try {
    await new Promise((accept, reject) => {
      const child = spawn(process.execPath, [dependencyExecutable('typescript', 'tsc', project), '-p', config,
        '--outDir', resolve(staging, 'assets/contracts'), '--pretty', 'false'], {cwd: project, stdio: 'pipe'});
      let output = '';
      child.stdout.on('data', data => output += data);
      child.stderr.on('data', data => output += data);
      child.once('error', reject);
      child.once('exit', code => code === 0 ? accept() : reject(new Error('Contracts compilation failed:\n' + output)));
    });
    const {buildRuntimeGenerationWorkerSource} = await import('./frontend-assets.mjs');
    await writeFile(resolve(staging, 'assets/contracts', workerName), await buildRuntimeGenerationWorkerSource({project}));
    const currentFiles = await sources(source);
    const current = await fingerprint([...currentFiles, ...inputs.slice(files.length)]);
    if (digest(JSON.stringify(current)) !== inputHash) {
      if (attempt >= 2) throw new Error('Contracts source changed repeatedly during compilation');
      return prepare(project, attempt + 1);
    }
    const paths = [...files.map(path => 'assets/contracts/' + relative(source, path).replaceAll('\\', '/').replace(/\.mts$/, '.mjs')),
      'assets/contracts/' + workerName];
    const outputs = Object.fromEntries(await Promise.all(paths.map(async path => [path, digest(await readFile(resolve(staging, path)))])));
    await writeFile(resolve(staging, 'manifest.json'), JSON.stringify({schema: 'eagler-touhou/contracts-build/1', inputHash, inputs: entries, outputs}) + '\n');
    try {await rename(staging, root);}
    catch (error) {
      // Independent builders may finish the same immutable generation together.
      if (!await valid(root, inputHash)) throw error;
    }
    return result;
  } finally {await rm(staging, {recursive: true, force: true});}
}

export function ensureContractsBuild({project = WEB_ROOT} = {}) {
  const key = resolve(project);
  if (!builds.has(key)) builds.set(key, prepare(key).finally(() => builds.delete(key)));
  return builds.get(key);
}

export async function resolveRuntimeGenerationWorkerSource({project = WEB_ROOT} = {}) {
  const build = await ensureContractsBuild({project});
  return resolve(build.contractsDirectory, workerName);
}
