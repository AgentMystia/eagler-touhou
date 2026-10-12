import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, cp, rm, readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { tmpdir } from 'node:os';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { WebSocket } from 'ws';
import { buildNetplayBundle } from '../bundle.mjs';

test('deployed relay runs with only its bundled modules and declared ws dependency', { timeout: 15000 }, async () => {
  const work = await mkdtemp(resolve(tmpdir(), 'netplay-bundle-'));
  let child, socket;
  try {
    await buildNetplayBundle(work);
    const manifest = JSON.parse(await readFile(resolve(work, 'package.json'), 'utf8'));
    assert.deepEqual(Object.keys(manifest.dependencies), ['ws']);
    const require = createRequire(import.meta.url);
    await cp(dirname(require.resolve('ws/package.json')), resolve(work, 'node_modules/ws'), { recursive: true });
    const env = { ...process.env };
    for (const key of Object.keys(env)) if (key.startsWith('EAGLER_NETPLAY_') || key.startsWith('TH07_')) delete env[key];
    child = spawn(process.execPath, ['server/netplay-relay.mjs'], {
      cwd: work, env: { ...env, EAGLER_NETPLAY_RELAY_HOST: '127.0.0.1', EAGLER_NETPLAY_RELAY_PORT: '0' },
      stdio: ['ignore', 'pipe', 'pipe'], windowsHide: true,
    });
    const exit = once(child, 'exit');
    let log = '';
    const port = await new Promise((resolvePort, reject) => {
      const timer = setTimeout(() => reject(new Error('Relay startup timed out: ' + log)), 5000);
      child.stderr.on('data', bytes => { log += bytes; });
      child.once('error', reject);
      child.once('exit', () => { clearTimeout(timer); reject(new Error(log)); });
      child.stdout.on('data', bytes => {
        log += bytes;
        const match = /listening ws:\/\/127\.0\.0\.1:(\d+)/.exec(log);
        if (match) { clearTimeout(timer); resolvePort(Number(match[1])); }
      });
    });
    socket = new WebSocket('ws://127.0.0.1:' + port + '/?room=th09mp-7341&lobby=bundle_client_001&member=private_bundle_member_001');
    const message = once(socket, 'message', { signal: AbortSignal.timeout(5000) });
    await once(socket, 'open', { signal: AbortSignal.timeout(5000) });
    const [data] = await message;
    const state = JSON.parse(String(data));
    assert.equal(state.type, 'state');
    assert.ok(state.room);
    socket.terminate();
    child.kill();
    await exit;
  } finally {
    socket?.terminate();
    if (child && child.exitCode === null && child.signalCode === null) {
      const exit = once(child, 'exit'); child.kill(); await exit;
    }
    await rm(work, { recursive: true, force: true });
  }
});
