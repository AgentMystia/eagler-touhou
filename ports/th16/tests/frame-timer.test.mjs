// 0x4262f0 FrameTimer / 0x417bc0 spell time encode
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const harness = process.env.TH16_HARNESS || join(root, 'tests/build/harness');

function run(args) {
  const r = spawnSync(harness, args, { encoding: 'utf8' });
  assert.equal(r.status, 0, r.stderr || r.stdout);
  return r.stdout.trim();
}

test('spell-time-encode roundtrips through 0x426840 checks', () => {
  for (const sec of ['0', '1.00', '12.34', '60.50', '999.99']) {
    const out = run(['spell-time-encode', sec]);
    assert.match(out, /ok=1/, out);
  }
});

test('frame-timer 0x4262f0: 120 logic frames → ~60fps window', () => {
  const out = run(['frame-timer', '120']);
  const p = Object.fromEntries(out.split(/\s+/).map((kv) => kv.split('=')));
  assert.ok(Number(p.fps) > 55 && Number(p.fps) < 65, out);
  assert.ok(Number(p.total) >= 60, out);
});
