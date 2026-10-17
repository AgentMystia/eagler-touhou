import { existsSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, readFile, readdir, rename, rm, writeFile, cp } from 'node:fs/promises';
import { dirname, relative, resolve, posix } from 'node:path';
import { spawn } from 'node:child_process';
import { resolvePublicOrRepositorySource } from './public-source.mjs';
import { ensureContractsBuild } from './contracts-build.mjs';
import { contractsSourceDirectory, dependencyExecutable, dependencyPath, sourceProjectRoot, WEB_ROOT } from './source-layout.mjs';
import { localModuleClosure, rewriteModuleSpecifiers } from './browser-module-graph.mjs';

const builds = new Map();
const digest = bytes => createHash('sha256').update(bytes).digest('hex');
const schema = 'eagler-touhou/client-services-build/1';

async function sourceFiles(directory) {
  const files = [];
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const path = resolve(directory, entry.name);
    if (entry.isDirectory()) files.push(...await sourceFiles(path));
    else if (entry.isFile() && entry.name.endsWith('.mts')) files.push(path);
  }
  return files.sort();
}

function logicalPath(value) {
  if (typeof value !== 'string' || !value || value.includes('\\') ||
      value.split('/').some(part => !part || part === '.' || part === '..') || /^[a-z]:/i.test(value)) {
    throw new Error('Invalid service output path: ' + value);
  }
  return value;
}

async function readManifest(root, inputHash) {
  try {
    const manifest = JSON.parse(await readFile(resolve(root, 'manifest.json'), 'utf8'));
    if (manifest.schema !== schema || manifest.inputHash !== inputHash ||
        !manifest.outputs || !Object.keys(manifest.outputs).length) return null;
    for (const [path, hash] of Object.entries(manifest.outputs)) {
      if (digest(await readFile(resolve(root, logicalPath(path)))) !== hash) return null;
    }
    return manifest;
  } catch (error) {
    if (error.code === 'ENOENT' || error instanceof SyntaxError) return null;
    throw error;
  }
}

function generation(root, manifest, sourceCount, built) {
  return Object.freeze({
    root, inputHash: manifest.inputHash, sourceCount, built,
    resolve(path) {
      logicalPath(path);
      if (!Object.hasOwn(manifest.outputs, path)) throw new Error('File is not in this service generation: ' + path);
      return resolve(root, path);
    },
  });
}

async function baseInputs(project) {
  const source = resolve(project, 'src');
  const contracts = contractsSourceDirectory(project);
  if (!existsSync(resolve(source, 'launcher')) || !contracts) {
    throw new Error('Client service compilation requires the Web and contracts source trees');
  }
  const files = await sourceFiles(source);
  const inputs = [
    ...files, ...await sourceFiles(contracts),
    ...await sourceFiles(resolve(project, 'package')), ...await sourceFiles(resolve(project, 'compat')),
    resolve(contracts, '../tsconfig.json'),
    resolve(project, 'tsconfig.launcher.json'), resolve(sourceProjectRoot(project), 'package-lock.json'),
    dependencyPath('typescript/package.json', project), dependencyPath('esbuild/package.json', project),
    dependencyPath('acorn/package.json', project),
    ...['client-services-build', 'contracts-build', 'source-layout', 'frontend-assets',
      'browser-module-graph', 'public-source'].map(name => resolve(project, 'lib', name + '.mjs')),
    resolve(project, 'compat/path-aliases.mjs'),
  ];
  const hashes = await Promise.all(inputs.map(async path => [
    relative(project, path).replaceAll('\\', '/'), digest(await readFile(path)),
  ]));
  return { files, hashes };
}

async function dependencies(project, paths) {
  return Promise.all(paths.map(async path => [
    logicalPath(path), digest(await readFile(resolvePublicOrRepositorySource(path, { project }))),
  ]));
}

function identity(base, deps) {
  return digest(JSON.stringify({ base, dependencies: deps }));
}

async function compile(project, staging) {
  await new Promise((accept, reject) => {
    const child = spawn(process.execPath, [
      dependencyExecutable('typescript', 'tsc', project), '-p', resolve(project, 'tsconfig.launcher.json'),
      '--outDir', resolve(staging, 'assets'), '--noEmit', 'false', '--pretty', 'false',
    ], { cwd: project, stdio: 'pipe' });
    let output = '';
    child.stdout.on('data', bytes => output += bytes);
    child.stderr.on('data', bytes => output += bytes);
    child.once('error', reject);
    child.once('exit', code => code === 0 ? accept() : reject(new Error('Client service compilation failed:\n' + output)));
  });
}

function bindContracts(source, path) {
  return rewriteModuleSpecifiers(source, specifier => {
    if (!specifier.startsWith('@eagler/contracts/')) return specifier;
    const target = 'assets/contracts/' + logicalPath(specifier.slice('@eagler/contracts/'.length)) + '.mjs';
    const result = posix.relative(posix.dirname(path), target);
    return result.startsWith('.') ? result : './' + result;
  });
}

async function prepare(project, force, attempt = 0) {
  const { files, hashes: base } = await baseInputs(project);
  const cache = resolve(project, '.cache/build/client-services');
  // This index only avoids rediscovering the JavaScript dependency graph.
  // Every input and output is checked; callers never resolve files through it.
  const index = resolve(cache, 'dependencies.json');
  if (!force) {
    try {
      const paths = JSON.parse(await readFile(index, 'utf8'));
      const deps = await dependencies(project, paths);
      const inputHash = identity(base, deps);
      const root = resolve(cache, inputHash);
      const manifest = await readManifest(root, inputHash);
      if (manifest) return generation(root, manifest, files.length, false);
      if (existsSync(root)) throw new Error('Client service generation is corrupt: ' + root);
    } catch (error) {
      if (error.code !== 'ENOENT' && !(error instanceof SyntaxError)) throw error;
    }
  }
  await mkdir(cache, { recursive: true });
  const staging = await mkdtemp(resolve(cache, '.staging-'));
  try {
    await compile(project, staging);
    const contracts = await ensureContractsBuild({ project });
    await cp(contracts.contractsDirectory, resolve(staging, 'assets/contracts'), { recursive: true });
    const entries = files.filter(path => !path.endsWith('.d.mts')).map(path =>
      'assets/' + relative(resolve(project, 'src'), path).replaceAll('\\', '/').replace(/\.mts$/, '.mjs'));
    for (const path of entries) {
      const target = resolve(staging, path);
      await writeFile(target, bindContracts(await readFile(target, 'utf8'), path));
    }
    const copied = new Map();
    const modules = await localModuleClosure({
      root: staging, entries, allowBareImports: true, allowDynamicImports: true,
      async resolveFile(path) {
        const target = resolve(staging, logicalPath(path));
        if (!existsSync(target)) {
          if (path.startsWith('assets/')) throw new Error('Compiled service dependency is missing: ' + path);
          const bytes = await readFile(resolvePublicOrRepositorySource(path, { project }));
          copied.set(path, digest(bytes));
          await mkdir(dirname(target), { recursive: true });
          await writeFile(target, bindContracts(bytes.toString('utf8'), path));
        }
        return target;
      },
    });
    const deps = [...copied].sort(([a], [b]) => a.localeCompare(b, 'en'));
    const current = await baseInputs(project);
    if (JSON.stringify(base) !== JSON.stringify(current.hashes) ||
        JSON.stringify(deps) !== JSON.stringify(await dependencies(project, deps.map(([path]) => path)))) {
      if (attempt >= 2) throw new Error('Client service inputs changed repeatedly during compilation');
      return await prepare(project, force, attempt + 1);
    }
    const contractPaths = Object.keys(JSON.parse(await readFile(resolve(contracts.root, 'manifest.json'), 'utf8')).outputs);
    const outputPaths = [...new Set([...modules, ...entries, ...contractPaths])].sort();
    const outputs = Object.fromEntries(await Promise.all(outputPaths.map(async path =>
      [path, digest(await readFile(resolve(staging, path)))])));
    const inputHash = identity(base, deps);
    const manifest = { schema, inputHash, inputs: { base, dependencies: deps }, outputs };
    await writeFile(resolve(staging, 'manifest.json'), JSON.stringify(manifest) + '\n');
    const root = resolve(cache, inputHash);
    try { await rename(staging, root); }
    catch (error) {
      // Concurrent builders may publish identical generations. No shared lock
      // or mutable output is involved, so a killed builder cannot block others.
      if (!await readManifest(root, inputHash)) throw error;
    }
    // Atomic, disposable optimization. A concurrent build may replace this list
    // with another valid source graph without changing any generation handle.
    const nextIndex = resolve(cache, '.dependencies-' + process.pid + '-' + inputHash + '.json');
    try {
      await writeFile(nextIndex, JSON.stringify(deps.map(([path]) => path)) + '\n');
      await rename(nextIndex, index);
    } finally { await rm(nextIndex, { force: true }); }
    return generation(root, manifest, files.length, true);
  } finally { await rm(staging, { recursive: true, force: true }); }
}

/** A fixed service/contract/dependency generation owned by this caller. */
export function ensureClientServicesBuild({ project = WEB_ROOT, force = false } = {}) {
  const key = resolve(project);
  if (!builds.has(key)) builds.set(key, prepare(key, force).finally(() => builds.delete(key)));
  return builds.get(key);
}
