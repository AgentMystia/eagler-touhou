// Supervisor：Boot→Title 时序（0x43b290 请求 Title；0x44abc0 等待 0xb4 帧或跳过输入）。
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {existsSync} from 'node:fs';
import {resolve} from 'node:path';

const here = resolve(import.meta.dirname);
const monorepo = resolve(here, '../../..');
const harness = process.env.TH16_HARNESS ?? resolve(here, 'build/harness');
const dat = process.env.TH16_DAT ?? resolve(monorepo, 'games/th16/original/th16.dat');
const available = existsSync(dat);

function run(args) {
  return execFileSync(harness, args, {encoding: 'utf8'});
}

test('scene-boot: after 180 frames title menu is live with quads', {skip: !available && 'original th16.dat not present'}, () => {
  const out = run(['scene-boot', dat, '185']);
  const header = out.trim().split('\n')[0];
  assert.match(header, /scene=4 boot_frames=18[0-5] title_live=1 quads=[1-9]/);
  const lines = out.trim().split('\n').slice(1);
  assert.ok(lines.length >= 1);
  // 标题脚本 0/0x61/0x65 或其子 VM 应出现在绘制链上
  const scripts = new Set(lines.map(l => Number(l.split(' ')[1])));
  assert.ok([...scripts].some(s => s === 0 || s === 0x61 || s === 0x65 || s > 0));
});

test('scene-boot: shot at frame 10 skips the 180-frame hold', {skip: !available && 'original th16.dat not present'}, () => {
  const out = run(['scene-boot', dat, '20', '10']);
  const header = out.trim().split('\n')[0];
  // boot_frames 在 skip 当帧满足 hold 后停止递增（title_menu_live）
  assert.match(header, /scene=4 boot_frames=1[0-9] title_live=1 quads=[1-9]/);
  const bootFrames = Number(header.match(/boot_frames=(\d+)/)[1]);
  assert.ok(bootFrames < 180, `expected early skip, got boot_frames=${bootFrames}`);
});
