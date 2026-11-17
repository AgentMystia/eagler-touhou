// Supervisor / Chain / 标题菜单：Boot→Title 时序（0x43b290 请求 Title；0x44abc0 等 0xb4 帧）与 0x44b5f0 状态机。
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
const needDat = {skip: !available && 'original th16.dat not present'};

function run(args) {
  return execFileSync(harness, args, {encoding: 'utf8'});
}

function parse(out) {
  const lines = out.trim().split('\n');
  const header = Object.fromEntries(lines[0].split(' ').map(kv => kv.split('=')).map(([k, v]) => [k, Number(v)]));
  const events = lines.filter(l => l.startsWith('event ')).map(l => l.split(' ').slice(1));
  const quads = lines.filter(l => /^-?\d+ \d+ \d+ /.test(l));
  return {header, events, quads};
}

// 位：0x1 Z，0x2 X，0x10 ↑，0x20 ↓，0x100 Esc，0x80000 Enter
const Z = '0x1', X = '0x2', UP = '0x10', DOWN = '0x20', ESC = '0x100', ENTER = '0x80000';
const MENU_LIVE = 180;       // 启动幕 +0x638 到 0xb4 的那一帧激活标题更新链
const MAIN = MENU_LIVE + 11; // Timer 0x78→0x83（>0x82）进入 case 2

test('scene-boot: boot hold is exactly 0xb4 draw-chain frames, then title decor/intro', needDat, () => {
  const before = parse(run(['scene-boot', dat, String(MENU_LIVE)]));
  assert.equal(before.header.title_live, 1);
  assert.equal(before.header.boot_frames, 180);
  assert.equal(before.header.menu_sub, 1, 'case 0 falls into case 1 (Intro) on the activation frame');
  assert.equal(before.header.draw_chains, 0, 'boot draw chain removed once the title thread unloads sig.anm');
  assert.equal(before.header.update_chains, 1, 'title update chain (prio 6) registered');
  const notYet = parse(run(['scene-boot', dat, String(MENU_LIVE - 1)]));
  assert.equal(notYet.header.title_live, 0);
  assert.equal(notYet.header.draw_chains, 1);
  assert.ok(before.header.quads >= 10, `title quads expected, got ${before.header.quads}`);
});

test('scene-boot: input cannot skip the boot hold; DAT_004c1800&0x180 can', needDat, () => {
  const held = parse(run(['scene-boot', dat, '40', `5:${Z},6:${Z},7:${ENTER}`]));
  assert.equal(held.header.title_live, 0);
  assert.equal(held.header.boot_frames, 40);
  const skipped = parse(run(['scene-boot', dat, '20', 'skip']));
  assert.equal(skipped.header.title_live, 1);
  assert.equal(skipped.header.boot_frames, 1);
  assert.equal(skipped.header.menu_sub, 2);
});

test('title menu: Main at Timer>0x82, input before that is ignored', needDat, () => {
  const early = parse(run(['scene-boot', dat, String(MAIN + 1), `${MAIN - 2}:${DOWN}`]));
  assert.equal(early.header.menu_sub, 2);
  assert.equal(early.header.cursor, 0, 'DOWN during Intro must not move the cursor');
  const atMain = parse(run(['scene-boot', dat, String(MAIN)]));
  assert.equal(atMain.header.menu_sub, 2);
  const beforeMain = parse(run(['scene-boot', dat, String(MAIN - 1)]));
  assert.equal(beforeMain.header.menu_sub, 1);
});

test('title menu: cursor wraps and skips disabled Extra (no clear data), SFX 10 per move', needDat, () => {
  const t = MAIN + 1;
  const down = parse(run(['scene-boot', dat, String(t + 4), `${t}:${DOWN}`]));
  assert.equal(down.header.cursor, 2, 'item 1 (Extra) is disabled by 0x440c00 when 0x44a850()==0');
  assert.deepEqual(down.events, [['sfx', '10']]);
  const up = parse(run(['scene-boot', dat, String(t + 4), `${t}:${UP}`]));
  assert.equal(up.header.cursor, 9, 'wrap=1 (+0xf4) → 0 − 1 → 9');
  const upTwice = parse(run(['scene-boot', dat, String(t + 6), `${t}:${UP},${t + 2}:${UP},${t + 4}:${UP}`]));
  assert.equal(upTwice.header.cursor, 7);
  const upToExtra = parse(run(['scene-boot', dat, String(t + 4), `${t}:${DOWN},${t + 2}:${UP}`]));
  assert.equal(upToExtra.header.cursor, 0, '2 − 1 = 1 is disabled → 0');
});

test('title menu: held DOWN auto-repeats after 0x19 frames every 8 frames (0x405580)', needDat, () => {
  const t = MAIN + 1;
  const script = Array.from({length: 40}, (_, i) => `${t + i}:${DOWN}`).join(',');
  const out = parse(run(['scene-boot', dat, String(t + 40), script]));
  // 按下沿 1 次 + 第 26 帧、34 帧连发 2 次 = 3 次移动：0→2→3→4
  assert.equal(out.events.filter(e => e[0] === 'sfx' && e[1] === '10').length, 3);
  assert.equal(out.header.cursor, 4);
});

test('title menu: confirm (Z|Enter) → sub 4, SFX 7, screen Difficulty(5) after 0x13 frames', needDat, () => {
  const t = MAIN + 1;
  const z = parse(run(['scene-boot', dat, String(t + 19), `${t}:${Z}`]));
  assert.equal(z.header.menu_sub, 4);
  assert.equal(z.header.selected, 0);
  assert.equal(z.header.next_screen, -1, 'Timer must exceed 0x13 first');
  assert.deepEqual(z.events, [['sfx', '7']]);
  const enter = parse(run(['scene-boot', dat, String(t + 21), `${t}:${ENTER}`]));
  assert.equal(enter.header.next_screen, 5);
  assert.deepEqual(enter.events, [['sfx', '7'], ['screen', '5']]);
});

test('title menu: cancel (X|Esc) jumps to Quit with SFX 9; cancel on Quit confirms Quit(2)', needDat, () => {
  const t = MAIN + 1;
  const esc = parse(run(['scene-boot', dat, String(t + 3), `${t}:${ESC}`]));
  assert.equal(esc.header.cursor, 9);
  assert.equal(esc.header.menu_sub, 2);
  assert.deepEqual(esc.events, [['sfx', '9']]);
  const quit = parse(run(['scene-boot', dat, String(t + 25), `${t}:${ESC},${t + 2}:${X}`]));
  assert.equal(quit.header.selected, 9);
  assert.equal(quit.header.next_screen, 2);
  assert.deepEqual(quit.events, [['sfx', '9'], ['sfx', '9'], ['screen', '2']]);
});

test('title menu: per-item target screens (0x44b5f0 case 4)', needDat, () => {
  const t = MAIN + 1;
  const expect = {2: 5, 3: 0x11, 4: 0xb, 5: 0xa, 6: 0xd, 7: 3, 8: 0x10};
  for (const [item, screen] of Object.entries(expect)) {
    const downs = Array.from({length: Number(item) - 1}, (_, i) => `${t + i * 2}:${DOWN}`);
    const confirmAt = t + (Number(item) - 1) * 2 + 2;
    const out = parse(run(['scene-boot', dat, String(confirmAt + 21), [...downs, `${confirmAt}:${Z}`].join(',')]));
    assert.equal(out.header.selected, Number(item), `item ${item}`);
    assert.equal(out.header.next_screen, screen, `item ${item} → screen ${screen}`);
  }
});

test('chain-demo: priority order, Remove, Restart, enable/disable', () => {
  assert.equal(run(['chain-demo']).trim(), 'BCCA|CA|CD r=1,1,1 count=3');
});

test('frame-clock: 60Hz jitter → 1 step, 120Hz → 0/1, 30Hz → 2, long stall → resync 1', () => {
  const sixty = run(['frame-clock', '1.000', '1.0165', '1.0335', '1.0501', '1.0668']).trim();
  assert.equal(sixty, '1,1,1,1,1');
  const onetwenty = run(['frame-clock', '1.000', '1.0083', '1.0167', '1.0250', '1.0333']).trim();
  assert.equal(onetwenty, '1,0,1,0,1');
  const thirty = run(['frame-clock', '1.000', '1.0333', '1.0667']).trim();
  assert.equal(thirty, '1,2,2');
  const stall = run(['frame-clock', '1.000', '1.0167', '5.0']).trim();
  assert.equal(stall, '1,1,1');
  const drift = run(['frame-clock', ...Array.from({length: 61}, (_, i) => String(1 + i * 0.0170))]).trim().split(',').map(Number);
  assert.equal(drift.reduce((a, b) => a + b, 0), 62, '17.0ms frames: one extra catch-up step over 60 frames');
});

test('input-edges: pressed on rising edge, repeat after 0x19 held frames then every 8, released on fall', () => {
  const held = ['0x20', '0x20', '0', ...Array.from({length: 34}, () => '0x20'), '0'];
  const rows = run(['input-edges', ...held]).trim().split('\n').map(l => l.split(' '));
  assert.deepEqual(rows[0], ['20', '0', '0']);
  assert.deepEqual(rows[1], ['0', '0', '0']);
  assert.deepEqual(rows[2], ['0', '0', '20']);
  assert.deepEqual(rows[3], ['20', '0', '0']);
  const repeatFrames = rows.map((r, i) => (r[1] === '20' ? i : -1)).filter(i => i >= 0);
  // 第 3 行重新按下算第 1 帧；第 26 帧（>0x19）首次连发，之后每 8 帧
  assert.deepEqual(repeatFrames, [3 + 25, 3 + 33]);
  assert.deepEqual(rows.at(-1), ['0', '0', '20']);
});
