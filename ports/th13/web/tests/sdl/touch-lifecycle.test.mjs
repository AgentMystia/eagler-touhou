import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import {dirname, join, resolve, extname} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createRequire} from 'node:module';

const port = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const runtime = join(port, 'web/sdl-runtime');
const build = process.env.TH13_SDL3_BUILD_DIR ?? join(port, 'web/artifacts');
const require = createRequire(import.meta.url);
let chromium;
try { ({chromium} = require('playwright-core')); }
catch { ({chromium} = require(join(process.env.NODE_PATH ?? '/opt/shot/node_modules', 'playwright-core'))); }

test('standalone mobile controls preserve fire across cancellation and input edges', {timeout: 180000}, async () => {
  const server = createServer(async (request, response) => {
    try {
      const path = new URL(request.url, 'http://localhost').pathname;
      const file = path.startsWith('/artifacts/') ? join(build, path.slice(11))
        : join(runtime, path.replace(/^\/sdl-runtime\//, ''));
      const data = await readFile(file);
      response.writeHead(200, {'Content-Type': ({'.mjs': 'text/javascript', '.wasm': 'application/wasm',
        '.html': 'text/html', '.png': 'image/png'})[extname(file)] ?? 'application/octet-stream'}).end(data);
    } catch { response.writeHead(404).end(); }
  });
  await new Promise(done => server.listen(0, '127.0.0.1', done));
  let browser;
  try {
    browser = await chromium.launch({executablePath: process.env.CHROMIUM_PATH, headless: true,
      args: ['--no-sandbox', '--disable-dev-shm-usage', '--use-angle=swiftshader', '--enable-unsafe-swiftshader']});
    const page = await browser.newPage({viewport: {width: 640, height: 480}, hasTouch: true, isMobile: true});
    await page.goto(`http://127.0.0.1:${server.address().port}/sdl-runtime/th13.html`);
    await page.waitForFunction(() => window.mod?._th13_frame() > 10, null, {timeout: 120000});
    await page.waitForSelector('[data-touch="fire"]');
    const result = await page.evaluate(() => {
      const mod = window.mod;
      const calls = [];
      for (const name of ['_th13_touch_controls', '_th13_touch_cancel', '_th13_touch', '_th13_key']) {
        const original = mod[name];
        mod[name] = (...args) => { calls.push([name, ...args]); return original(...args); };
      }
      const pointer = (node, type, id = 1) => node.dispatchEvent(new PointerEvent(type, {
        bubbles: true, cancelable: true, pointerType: 'touch', pointerId: id,
        clientX: 200, clientY: 200,
      }));
      const button = name => document.querySelector(`[data-touch="${name}"]`);
      const press = name => { pointer(button(name), 'pointerdown'); pointer(button(name), 'pointerup'); };
      const fire = button('fire');
      const initial = fire.getAttribute('aria-pressed');
      window.dispatchEvent(new Event('blur'));
      const afterBlur = fire.getAttribute('aria-pressed');
      const lastControls = calls.filter(c => c[0] === '_th13_touch_controls').at(-1);
      press('fire');
      const off = fire.getAttribute('aria-pressed');
      window.dispatchEvent(new Event('blur'));
      const offAfterBlur = fire.getAttribute('aria-pressed');
      press('fire'); press('pause'); press('bomb'); press('trance'); press('retry');
      const canvas = document.querySelector('canvas');
      pointer(canvas, 'pointerdown', 7); pointer(canvas, 'pointermove', 7); pointer(canvas, 'pointerup', 7);
      return {initial, afterBlur, lastControls, off, offAfterBlur, calls};
    });
    assert.equal(result.initial, 'true');
    assert.equal(result.afterBlur, 'true');
    assert.equal(result.lastControls[2], 1);
    assert.equal(result.off, 'false');
    assert.equal(result.offAfterBlur, 'false');
    for (const [scan, down] of [[46, 1], [46, 0], [19, 1], [19, 0]])
      assert.ok(result.calls.some(c => c[0] === '_th13_key' && c[1] === scan && c[2] === down));
    assert.ok(result.calls.some(c => c[0] === '_th13_touch_controls' && c[4] === 1));
    assert.ok(result.calls.some(c => c[0] === '_th13_touch_controls' && c[5] === 1));
    assert.deepEqual(result.calls.filter(c => c[0] === '_th13_touch').map(c => c[1]), [0, 1, 2]);
  } finally {
    await browser?.close();
    await new Promise(done => server.close(done));
  }
});

test('mobile auto fire survives pause, canvas resume and retry in a real run', {timeout: 180000}, async () => {
  const server = createServer(async (request, response) => {
    try {
      const path = new URL(request.url, 'http://localhost').pathname;
      const file = path.startsWith('/artifacts/') ? join(build, path.slice(11))
        : join(runtime, path.replace(/^\/sdl-runtime\//, ''));
      const data = await readFile(file);
      response.writeHead(200, {'Content-Type': ({'.mjs': 'text/javascript', '.wasm': 'application/wasm',
        '.html': 'text/html', '.png': 'image/png'})[extname(file)] ?? 'application/octet-stream'}).end(data);
    } catch { response.writeHead(404).end(); }
  });
  await new Promise(done => server.listen(0, '127.0.0.1', done));
  let browser;
  try {
    browser = await chromium.launch({executablePath: process.env.CHROMIUM_PATH, headless: true,
      args: ['--no-sandbox', '--disable-dev-shm-usage', '--use-angle=swiftshader', '--enable-unsafe-swiftshader']});
    const page = await browser.newPage({viewport: {width: 640, height: 480}, hasTouch: true, isMobile: true});
    await page.goto(`http://127.0.0.1:${server.address().port}/sdl-runtime/th13.html`);
    await page.waitForFunction(() => window.mod?._th13_frame() > 10, null, {timeout: 120000});
    const result = await page.evaluate(() => {
      const mod = window.mod;
      // Stop RAF without calling loop_stop(), which deliberately clears input.
      mod._th13_loop_pause(1);
      const state = () => [...mod.HEAP32.subarray(mod._th13_probe_state() / 4, mod._th13_probe_state() / 4 + 16)];
      const step = (n = 1) => { for (let i = 0; i < n; ++i)
        if (!mod._th13_debug_frame()) throw Error(mod.UTF8ToString(mod._th13_error())); };
      const until = (predicate, n, name) => { for (let i = 0; i < n; ++i) { if (predicate()) return; step(); }
        throw Error(`${name}: ${JSON.stringify(state())}`); };
      const key = scan => { mod._th13_key(scan, 1); step(); mod._th13_key(scan, 0); step(); };
      const touch = (node, type, id = 1) => node.dispatchEvent(new PointerEvent(type, {
        bubbles: true, cancelable: true, pointerType: 'touch', pointerId: id, clientX: 200, clientY: 200,
      }));
      const button = name => { const node = document.querySelector(`[data-touch="${name}"]`);
        touch(node, 'pointerdown'); touch(node, 'pointerup'); };
      const tapCanvas = () => { const canvas = document.querySelector('canvas');
        touch(canvas, 'pointerdown', 7); touch(canvas, 'pointerup', 7); };
      const report = () => mod.UTF8ToString(mod._th13_debug_stage_report());
      const generations = () => [...report().matchAll(/player_shot gen=(\d+)/g)].map(m => Number(m[1]));
      const newest = () => Math.max(0, ...generations());
      const fire = document.querySelector('[data-touch="fire"]');
      if (fire.getAttribute('aria-pressed') !== 'true') throw Error('auto fire initially disabled');
      // Count calls from here onward to detect any control re-send after resume/retry.
      const original = mod._th13_touch_controls; let sends = 0;
      mod._th13_touch_controls = (...args) => { ++sends; return original(...args); };
      until(() => state()[0] === 0 && state()[8] === 1 && state()[9] === 2, 500, 'main');
      key(44); step(35); key(44); step(25); key(44);
      until(() => state()[0] === 1, 180, 'run');
      until(() => newest() > 0, 160, 'initial shots');
      const beforeResume = newest();
      button('pause'); step(20);
      if (state()[14] === 0) throw Error('pause did not open');
      const sendsAtPause = sends;
      tapCanvas();
      until(() => state()[14] === 0, 70, 'resume');
      until(() => newest() > beforeResume, 120, 'resumed shots');
      const sendsAtResume = sends;
      step(15); // Allow the menu-confirm and pause pulses to release.
      button('pause'); step(20);
      if (state()[14] === 0) throw Error('second pause did not open');
      const retry = document.querySelector('[data-touch="retry"]');
      touch(retry, 'pointerdown'); step(); touch(retry, 'pointerup'); step(25);
      key(200); key(44);
      until(() => state()[14] === 0 && state()[5] < 20, 150, 'retry');
      until(() => newest() > 0, 160, 'retry shots');
      return {sendsAtPause, sendsAtResume, sendsAtEnd: sends, state: state(), report: report()};
    });
    assert.equal(result.sendsAtResume, result.sendsAtPause, 'canvas resume did not resend fire state');
    assert.equal(result.sendsAtEnd, result.sendsAtResume + 1, 'only the second pause button sent controls');
    assert.match(result.report, /player_shot gen=/);
  } finally {
    await browser?.close();
    await new Promise(done => server.close(done));
  }
});
