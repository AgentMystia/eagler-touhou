// 0x46d990 / 0x46dab0 逻辑侧：flags_lo bit0 + 字号表 DAT_00491b68
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const harness = process.env.TH16_HARNESS || join(root, 'tests/build/harness');

test('text-paint: 0x46d990 sets flags_lo bit0; font heights match DAT_00491b68', () => {
  const r = spawnSync(harness, ['text-paint-flag'], { encoding: 'utf8' });
  assert.equal(r.status, 0, r.stderr || r.stdout);
  assert.match(r.stdout, /flag=1 queued=0 height0=17 height8=14/);
});
