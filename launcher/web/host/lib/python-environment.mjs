import { createHash, randomUUID } from 'node:crypto';
import { isDeepStrictEqual } from 'node:util';
import { existsSync } from 'node:fs';
import { mkdir, readFile, rename, rm, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { run } from './process.mjs';

const pending = new Map();
const digest = bytes => createHash('sha256').update(bytes).digest('hex');
function environmentPython(directory) {
  return process.platform === 'win32' ? resolve(directory, 'Scripts/python.exe') : resolve(directory, 'bin/python');
}

async function prepare({projectRoot, hostRoot, python, wheelhouse}) {
  const inspector = resolve(projectRoot, 'host/lib/python-environment.py');
  const lock = resolve(projectRoot, 'host/requirements.txt');
  const [requirements, inspectionSource] = await Promise.all([readFile(lock), readFile(inspector)]);
  const specification = JSON.parse(requirements.toString('utf8').split(/\r?\n/).find(line => line.startsWith('# environment: ')).slice(15));
  const probe = await run(python, ['-I', inspector, 'identity'], {capture: true, allowFailure: true});
  if (probe.code !== 0) throw new Error('Cannot inspect the configured Python interpreter: ' + probe.stderr);
  const identity = JSON.parse(probe.stdout), version = identity.version;
  const {min, maxExclusive} = specification.python;
  const compare = (a, b) => a[0] - b[0] || a[1] - b[1];
  if (identity.implementation !== 'cpython' || compare(version, min) < 0 || compare(version, maxExclusive) >= 0)
    throw new Error('Host requires CPython ' + min.join('.') + ' through ' + maxExclusive[0] + '.' + (maxExclusive[1] - 1));
  const key = digest(Buffer.concat([requirements, inspectionSource, Buffer.from(JSON.stringify(identity))]));
  const cache = resolve(hostRoot, '.cache/python-environments'), index = resolve(cache, 'active-' + key + '.json');
  try {
    const name = JSON.parse(await readFile(index, 'utf8')).directory;
    if (!/^[a-f0-9-]{36}$/.test(name)) throw new Error('Invalid Python environment index');
    const directory = resolve(cache, name), executable = environmentPython(directory);
    if (existsSync(executable)) {
      const checked = await run(executable, ['-I', inspector, 'verify', lock, resolve(directory, 'environment.json')],
        {capture: true, allowFailure: true});
      if (checked.code === 0 && isDeepStrictEqual(JSON.parse(checked.stdout).runtime, identity))
        return executable;
    }
  } catch (error) {
    if (error.code !== 'ENOENT' && !(error instanceof SyntaxError)) throw error;
  }

  // Virtual environments embed their own paths. Build each at its final unique
  // location and publish only a disposable index after complete verification.
  // No existing environment is repaired in place or held behind a shared lock.
  await mkdir(cache, {recursive: true});
  const name = randomUUID(), directory = resolve(cache, name), executable = environmentPython(directory);
  console.log('[Build] Creating a locked Python environment');
  let ready = false;
  try {
    await run(python, ['-I', '-m', 'venv', '--without-pip', directory], {capture: true});
    await run(executable, ['-I', '-m', 'ensurepip', '--upgrade', '--default-pip'], {capture: true});
    const offline = wheelhouse ? ['--no-index', '--find-links', resolve(wheelhouse)] : [];
    await run(executable, ['-I', '-m', 'pip', 'install', '--disable-pip-version-check',
      '--require-virtualenv', '--no-compile', ...offline, '-r', lock], {capture: true});
    const checked = await run(executable, ['-I', inspector, 'snapshot', lock], {capture: true});
    await writeFile(resolve(directory, 'environment.json'), checked.stdout + '\n');
    ready = true;
    const nextIndex = resolve(cache, '.active-' + name + '.json');
    try {
      await writeFile(nextIndex, JSON.stringify({directory: name}) + '\n');
      await rename(nextIndex, index);
    } finally { await rm(nextIndex, {force: true}); }
    return executable;
  } finally {
    if (!ready) await rm(directory, {recursive: true, force: true});
  }
}

export function ensurePythonEnvironment(options) {
  const input = {...options, python: options.python ?? (process.platform === 'win32' ? 'python' : 'python3')};
  const key = JSON.stringify([resolve(input.projectRoot), resolve(input.hostRoot), input.python, input.wheelhouse]);
  if (!pending.has(key)) pending.set(key, prepare(input).finally(() => pending.delete(key)));
  return pending.get(key);
}
