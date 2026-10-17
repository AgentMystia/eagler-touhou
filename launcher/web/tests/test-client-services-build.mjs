import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile, readdir, rm, symlink, writeFile } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { setTimeout as delay } from 'node:timers/promises';

const project = fileURLToPath(new URL('../', import.meta.url));
function processBuild(cold, force = false) {
  const child = spawn(process.execPath, ['--input-type=module', '-e',
    `import {ensureClientServicesBuild} from './lib/client-services-build.mjs';
     const b=await ensureClientServicesBuild({force:${force}});
     const m=await import(new URL('file:///'+b.resolve('assets/launcher/probe.mjs').replaceAll('\\\\','/')));
     console.log(JSON.stringify({root:b.root,hash:b.inputHash,value:m.read()}));`],
  { cwd: cold, stdio: 'pipe' });
  let output = '', errors = '';
  const done = new Promise((yes, no) => {
    child.stdout.on('data', data => output += data);
    child.stderr.on('data', data => errors += data);
    child.once('error', no);
    child.once('exit', (code, signal) => yes({code, signal, output, errors}));
  });
  return {child, done};
}
async function result(run) {
  const value = await run.done;
  assert.equal(value.code, 0, value.errors + value.output);
  return JSON.parse(value.output);
}
async function waitForStage(cold, known = []) {
  const cache = resolve(cold, '.cache/build/client-services');
  for (const deadline = Date.now() + 15000; Date.now() < deadline;) {
    const entries = await readdir(cache).catch(error => error.code === 'ENOENT' ? [] : Promise.reject(error));
    const stage = entries.find(name => name.startsWith('.staging-') && !known.includes(name));
    if (stage) return stage;
    await delay(20);
  }
  throw new Error('Builder did not enter its private staging directory');
}

test('service generations survive concurrent builders, changing inputs and a killed publisher', async () => {
  const work = await mkdtemp(resolve(project, '.cache/client-services-test-'));
  const cold = resolve(work, 'source/launcher/web');
  try {
    for (const path of ['lib', 'compat', 'package.json', 'tsconfig.launcher.json']) {
      await mkdir(resolve(cold, path, '..'), {recursive: true});
      await cp(resolve(project, path), resolve(cold, path), {recursive: true});
    }
    for (const path of ['contracts', 'package.json', 'package-lock.json']) {
      await cp(resolve(project, '../..', path), resolve(cold, '../..', path), {recursive: true});
    }
    await mkdir(resolve(cold, 'src/launcher'), {recursive: true});
    await mkdir(resolve(cold, 'package'), {recursive: true});
    await mkdir(resolve(cold, 'node_modules/@eagler'), {recursive: true});
    await symlink(resolve(project, '../../node_modules'), resolve(cold, '../../node_modules'), process.platform === 'win32' ? 'junction' : 'dir');
    await symlink(resolve(cold, '../../contracts'), resolve(cold, 'node_modules/@eagler/contracts'), process.platform === 'win32' ? 'junction' : 'dir');
    const probe = resolve(cold, 'src/launcher/probe.mts');
    const contract = resolve(cold, '../../contracts/src/build-probe.mts');
    const dependency = resolve(cold, 'package/dependency.mjs');
    await writeFile(contract, 'export const contractValue = "contract-a";\n');
    await writeFile(dependency, 'export const dependencyValue = "js-a";\n');
    await writeFile(resolve(cold, 'package/dependency.d.mts'), 'export const dependencyValue: string;\n');
    await writeFile(resolve(cold, 'package/lazy.mjs'), 'export const value = "lazy-a";\n');
    await writeFile(resolve(cold, 'package/lazy.d.mts'), 'export const value: string;\n');
    const initial = `import {contractValue} from '@eagler/contracts/build-probe';
      import {dependencyValue} from '../../package/dependency.mjs';
      export const read = () => [contractValue, dependencyValue, 'service-a'];
      export const lazy = () => import('../../package/lazy.mjs');\n`;
    await writeFile(probe, initial);
    const concurrent = await Promise.all([result(processBuild(cold)), result(processBuild(cold))]);
    assert.equal(concurrent[0].hash, concurrent[1].hash);
    assert.deepEqual(concurrent[0].value, ['contract-a', 'js-a', 'service-a']);
    const api = await import(pathToFileURL(resolve(cold, 'lib/client-services-build.mjs')).href);
    const first = await api.ensureClientServicesBuild();
    const old = await import(pathToFileURL(first.resolve('assets/launcher/probe.mjs')).href);

    await writeFile(contract, 'export const contractValue = "contract-b";\n');
    await writeFile(dependency, 'export const dependencyValue = "js-b";\n');
    await writeFile(resolve(cold, 'package/lazy.mjs'), 'export const value = "lazy-b";\n');
    const second = await api.ensureClientServicesBuild();
    const current = await import(pathToFileURL(second.resolve('assets/launcher/probe.mjs')).href);
    assert.notEqual(second.root, first.root);
    assert.deepEqual(current.read(), ['contract-b', 'js-b', 'service-a']);
    assert.deepEqual(old.read(), ['contract-a', 'js-a', 'service-a']);
    assert.equal((await old.lazy()).value, 'lazy-a', 'deferred imports retain the callers generation');
    assert.equal((await current.lazy()).value, 'lazy-b');
    assert.throws(() => second.resolve('../package.json'));
    assert.throws(() => second.resolve('assets/launcher/missing.mjs'));

    // Change source after a child has captured its inputs and entered compilation.
    const changing = processBuild(cold, true);
    await waitForStage(cold);
    await writeFile(probe, initial.replace('service-a', 'service-b'));
    const changed = await result(changing);
    assert.deepEqual(changed.value, ['contract-b', 'js-b', 'service-b']);
    assert.deepEqual(old.read(), ['contract-a', 'js-a', 'service-a']);

    // An interrupted publisher leaves only a private stage. Another process can
    // compile and publish immediately; no ownerless lock needs manual removal.
    const killed = processBuild(cold, true);
    const abandoned = await waitForStage(cold);
    killed.child.kill('SIGKILL');
    const stopped = await killed.done;
    assert.notEqual(stopped.code, 0);
    await writeFile(probe, initial.replace('service-a', 'service-c'));
    const recovered = await result(processBuild(cold));
    assert.deepEqual(recovered.value, ['contract-b', 'js-b', 'service-c']);
    assert.notEqual(recovered.hash, changed.hash);
    const recoveredApi = await api.ensureClientServicesBuild();
    assert.equal((await old.lazy()).value, 'lazy-a');
    assert.equal((await readdir(resolve(cold, '.cache/build/client-services'))).includes(abandoned), true);
    await writeFile(recoveredApi.resolve('assets/launcher/probe.mjs'), 'export const read=()=>["corrupt"];\n');
    await assert.rejects(api.ensureClientServicesBuild(), /corrupt/i);
  } finally {
    await rm(work, {recursive: true, force: true, maxRetries: 20, retryDelay: 100});
  }
});
