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
const Z = '0x1', X = '0x2', UP = '0x10', DOWN = '0x20', LEFT = '0x40', RIGHT = '0x80', ESC = '0x100', ENTER = '0x80000';
const MENU_LIVE = 180;       // 启动幕 +0x638 到 0xb4 的那一帧激活标题更新链
const MAIN = MENU_LIVE + 11; // Timer 0x78→0x83（>0x82）进入 case 2

test('scene-boot: boot hold is exactly 0xb4 draw-chain frames, then title decor/intro', needDat, () => {
  const before = parse(run(['scene-boot', dat, String(MENU_LIVE)]));
  assert.equal(before.header.title_live, 1);
  assert.equal(before.header.boot_frames, 180);
  assert.equal(before.header.screen, 1, '0x44af80 case 0 → 0x44a560(1) main menu on the activation frame');
  assert.equal(before.header.sub, 1, '0x44b5f0 case 0 falls into case 1 (Intro)');
  assert.equal(before.header.draw_chains, 0, 'boot draw chain removed once the title thread unloads sig.anm');
  assert.equal(before.header.update_chains, 2, 'EffectManager chain (prio 0x1f, 0x4187d0 at boot) + title update chain (prio 6) registered');
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
  assert.equal(skipped.header.sub, 2);
});

test('title menu: Main at Timer>0x82, input before that is ignored', needDat, () => {
  const early = parse(run(['scene-boot', dat, String(MAIN + 1), `${MAIN - 2}:${DOWN}`]));
  assert.equal(early.header.sub, 2);
  assert.equal(early.header.cursor, 0, 'DOWN during Intro must not move the cursor');
  const atMain = parse(run(['scene-boot', dat, String(MAIN)]));
  assert.equal(atMain.header.sub, 2);
  const beforeMain = parse(run(['scene-boot', dat, String(MAIN - 1)]));
  assert.equal(beforeMain.header.sub, 1);
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

test('title menu: confirm Game Start → sub 4, SFX 7, Difficulty(5) after 0x13 frames with cursor = last difficulty', needDat, () => {
  const t = MAIN + 1;
  const z = parse(run(['scene-boot', dat, String(t + 19), `${t}:${Z}`]));
  assert.equal(z.header.sub, 4);
  assert.equal(z.header.screen, 1, 'Timer must exceed 0x13 first');
  assert.deepEqual(z.events, [['sfx', '7']]);
  const enter = parse(run(['scene-boot', dat, String(t + 21), `${t}:${ENTER}`]));
  assert.equal(enter.header.screen, 5);
  assert.equal(enter.header.cursor, 1, 'DAT_0049f274 initial = 1 (Normal)');
  assert.equal(enter.header.difficulty, 1);
  assert.deepEqual(enter.events, [['sfx', '7'], ['screen', '5']]);
});

test('title menu: cancel (X|Esc) jumps to Quit with SFX 9; cancel on Quit confirms Quit → scene 2 request', needDat, () => {
  const t = MAIN + 1;
  const esc = parse(run(['scene-boot', dat, String(t + 3), `${t}:${ESC}`]));
  assert.equal(esc.header.cursor, 9);
  assert.equal(esc.header.sub, 2);
  assert.deepEqual(esc.events, [['sfx', '9']]);
  const quit = parse(run(['scene-boot', dat, String(t + 27), `${t}:${ESC},${t + 2}:${X}`]));
  assert.equal(quit.header.scene, 2, 'Supervisor consumed DAT_004c17c4 = 2');
  assert.equal(quit.header.quit, 1);
  assert.equal(quit.header.title_live, 0, 'title object destroyed on scene switch');
  assert.deepEqual(quit.events, [['sfx', '9'], ['sfx', '9'], ['screen', '2'], ['scene', '2']]);
});

test('title menu: per-item target screens (0x44b5f0 case 4)', needDat, () => {
  const t = MAIN + 1;
  const expect = {2: 5, 3: 0x11, 4: 0xb, 5: 0xa, 6: 0xd, 7: 3, 8: 0x10};
  for (const [item, screen] of Object.entries(expect)) {
    const downs = Array.from({length: Number(item) - 1}, (_, i) => `${t + i * 2}:${DOWN}`);
    const confirmAt = t + (Number(item) - 1) * 2 + 2;
    const out = parse(run(['scene-boot', dat, String(confirmAt + 21), [...downs, `${confirmAt}:${Z}`].join(',')]));
    assert.equal(out.header.screen, screen, `item ${item} → screen ${screen}`);
    if (item === '2') assert.equal(out.header.mode, 1, 'Practice sets mode bits 0x10');
    if (item === '3') assert.equal(out.header.mode, 2, 'Spell Practice sets mode bits 0x20');
  }
});

// 画面切换节拍（确定帧 C → 下一画面可接受输入的帧）：
//   主菜单 C+20 set_screen，C+21 case0，C+28 sub2 → C+29 可输入
//   难度 / 机体 C+14 set_screen，C+15 case0，C+22 sub2 → C+23
//   季节（练习）C+10 set_screen(8)，C+11 case0，C+22 sub2 → C+23
//   取消 C → C+6 set_screen，C+7 case0（sub 1）
const AFTER_MAIN = 29, AFTER_DIFF = 23, AFTER_SHOT = 23, AFTER_SEASON_PRACTICE = 23, AFTER_CANCEL = 7;
const DIFF_READY = 7;
test('difficulty screen: ←/→ and ↑/↓ move (no wrap), confirm → Shot(6) with difficulty saved', needDat, () => {
  const t = MAIN + 1;
  const enterDiff = t + 21;                       // 第一帧 screen=5（case 0）
  const nav = enterDiff + DIFF_READY + 1;
  const down = parse(run(['scene-boot', dat, String(nav + 2), `${t}:${Z},${nav}:${DOWN}`]));
  assert.equal(down.header.cursor, 2, 'Normal → Hard');
  const downThrice = parse(run(['scene-boot', dat, String(nav + 6), `${t}:${Z},${nav}:${DOWN},${nav + 2}:${DOWN},${nav + 4}:${DOWN}`]));
  assert.equal(downThrice.header.cursor, 3, 'wrap=0: clamps at Lunatic');
  const upTwice = parse(run(['scene-boot', dat, String(nav + 4), `${t}:${Z},${nav}:${LEFT},${nav + 2}:${UP}`]));
  assert.equal(upTwice.header.cursor, 0, 'LEFT also moves −1; clamps at Easy');
  const confirm = parse(run(['scene-boot', dat, String(nav + 2 + 15), `${t}:${Z},${nav}:${DOWN},${nav + 2}:${Z}`]));
  assert.equal(confirm.header.screen, 6);
  assert.equal(confirm.header.difficulty, 2);
  assert.equal(confirm.header.cursor, 0, 'shot cursor = DAT_004a6f24 (last shot) = 0');
  const sfx = confirm.events.filter(e => e[0] === 'sfx').map(e => e[1]);
  assert.deepEqual(sfx, ['7', '10', '7']);
  const notYet = parse(run(['scene-boot', dat, String(nav + 2 + 14), `${t}:${Z},${nav}:${DOWN},${nav + 2}:${Z}`]));
  assert.equal(notYet.header.screen, 5, 'Timer must exceed 0xd');
});

test('difficulty screen: cancel → back to main menu with difficulty = cursor, main menu restarts at Intro', needDat, () => {
  const t = MAIN + 1;
  const nav = t + 21 + DIFF_READY + 1;
  const back = parse(run(['scene-boot', dat, String(nav + 2 + AFTER_CANCEL), `${t}:${Z},${nav}:${DOWN},${nav + 2}:${X}`]));
  assert.equal(back.header.screen, 1);
  assert.equal(back.header.sub, 0, 'set_screen(1) on cancel+6');
  const out = parse(run(['scene-boot', dat, String(nav + 2 + AFTER_CANCEL + 1), `${t}:${Z},${nav}:${DOWN},${nav + 2}:${X}`]));
  assert.equal(out.header.screen, 1);
  assert.equal(out.header.difficulty, 2);
  assert.equal(out.header.sub, 1, 'main menu re-enters case 0 → Intro');
  assert.equal(out.header.cursor, 0, 'cursor popped to Game Start');
  assert.deepEqual(out.events.at(-1), ['screen', '1']);
});

test('game start flow: Normal / Cirno / Autumn → scene 7 with stage 1, title destroyed', needDat, () => {
  const t = MAIN + 1;
  const diff = t + AFTER_MAIN;
  const shot = diff + AFTER_DIFF;
  const season = shot + 2 + AFTER_SHOT;           // RIGHT 在 shot，确定在 shot+2
  const script = [`${t}:${Z}`, `${diff}:${Z}`, `${shot}:${RIGHT}`, `${shot + 2}:${Z}`, `${season}:${RIGHT}`, `${season + 2}:${Z}`].join(',');
  const atSeason = parse(run(['scene-boot', dat, String(season), script]));
  assert.equal(atSeason.header.screen, 7);
  assert.equal(atSeason.header.shot, 1, 'Cirno');
  assert.equal(atSeason.header.cursor, 1, 'season cursor starts at MenuCursor::set(shot)');
  const confirmed = parse(run(['scene-boot', dat, String(season + 2 + 12), script]));
  assert.equal(confirmed.header.screen, 7);
  assert.equal(confirmed.header.sub, 3);
  assert.ok(confirmed.events.some(e => e[0] === 'transition'), 'Timer==10 spawns the transition effect');
  assert.ok(confirmed.events.some(e => e[0] === 'bgm-fade'));
  const started = parse(run(['scene-boot', dat, String(season + 2 + 0x28 + 2), script]));
  assert.equal(started.header.scene, 7);
  assert.equal(started.header.title_live, 0);
  assert.equal(started.header.difficulty, 1);
  assert.equal(started.header.shot, 1);
  assert.equal(started.header.season, 2, 'Autumn');
  assert.equal(started.header.stage, 1);
  assert.equal(started.header.update_chains, 7, 'title update chain removed; EffectManager(0x1f, boot) + GameManager(0xf)+StageManager(0x11)+Replay 记录(0x10)+Player(0x16)+Bullet(0x1c)+Enemy(0x1a) registered (0x4187d0/0x42d700/0x409db0/0x447fd0/0x440fb0/0x411dd0/0x41b340)');
  // 0x44ad20：title.anm / title_v.anm VM 全部清掉；ascii.anm 落叶发射器（+0x73c）只收到 interrupt 1，
  // 其脚本 case 1 在 10/14 帧后自删，所以切场后短暂只剩槽 2 的落叶（mode 8）
  assert.ok(started.quads.length > 0, 'leaf particles (ascii.anm mode 8) still falling right after the switch');
  // 加上新游戏在装载期（0x4440e0）就生成的子机 VM：pl02.anm（槽 9，琪露诺脚本 7）
  const leaves = started.quads.filter(l => / slot=2$/.test(l));
  const others = started.quads.filter(l => !/ slot=2$/.test(l));
  assert.ok(leaves.length > 0, 'leaf particles still present');
  assert.ok(others.length > 0 && others.every(l => /^\d+ 7 .* slot=9$/.test(l)), 'only ascii.anm leaves + player option (pl02.anm script 7) remain: ' + others.slice(0, 3).join(' | '));
  assert.ok(leaves.every(l => / zw=1 /.test(l)), 'mode 8 writes Z (0x43c530)');
  assert.deepEqual(started.events.at(-1), ['scene', '7']);
  const settled = parse(run(['scene-boot', dat, String(season + 2 + 0x28 + 2 + 16), script]));
  assert.ok(settled.quads.every(l => / slot=9$/.test(l)), 'emitter (interrupt 1 → ins_1 after 14 frames) and its leaves are gone; only the player option remains');
  assert.ok(settled.quads.length > 0, 'player option VM keeps drawing');
});

test('season panel (title script 145, mode 8): Y-flip through the UI camera, then settles on the mode-0 rectangle', needDat, () => {
  const t = MAIN + 1;
  const diff = t + AFTER_MAIN;
  const shot = diff + AFTER_DIFF;
  const season = shot + 2 + AFTER_SHOT;
  const script = [`${t}:${Z}`, `${diff}:${Z}`, `${shot}:${RIGHT}`, `${shot + 2}:${Z}`].join(',');
  const panel = frame => {
    const out = parse(run(['scene-boot', dat, String(frame), script]));
    const line = out.quads.find(l => /^26 145 /.test(l));
    assert.ok(line, `script 145 drawn at frame ${frame}`);
    const [, , , x0, y0, x1, y3] = line.split(' ').map(Number);
    const z = line.match(/z=([^ ]+)/)[1].split(',').map(Number);
    const rhw = Number(line.match(/rhw=([^ ]+)/)[1]);
    const zw = Number(line.match(/zw=(\d)/)[1]);
    return {x0, y0, x1, y3, z, rhw, zw};
  };
  // ins_401 y=π 起、20 帧转到 0：首帧几乎侧对相机（左右边翻转、宽度坍缩）
  const edge = panel(season);
  assert.ok(edge.x1 < edge.x0 && Math.abs(edge.x0 - edge.x1) < 20, `edge-on: ${JSON.stringify(edge)}`);
  assert.equal(edge.zw, 1, 'mode 8 writes Z');
  // 中途：透视使左右 rhw/z 不同（0x43c780 相机 eye z = 240/tan(15°)）
  const mid = panel(season + 5);
  assert.ok(mid.x0 < mid.x1 && Math.abs(mid.z[0] - mid.z[1]) > 1e-4, `perspective mid-flip: ${JSON.stringify(mid)}`);
  // 转到 0 后与 mode 0 未对齐前的矩形一致：pos(640,660)×0.5 ± 精灵 619×205（weapon01b）×0.5 /2
  const flat = panel(season + 10);
  assert.ok(Math.abs(flat.x0 - 165.25) < 1e-3 && Math.abs(flat.x1 - 474.75) < 1e-3, `flat x: ${flat.x0}..${flat.x1}`);
  assert.ok(Math.abs(flat.y0 - 278.75) < 1e-3 && Math.abs(flat.y3 - 381.25) < 1e-3, `flat y: ${flat.y0}..${flat.y3}`);
  const d = 240 / Math.tan(Math.PI / 12);
  assert.ok(Math.abs(flat.rhw - 1 / d) < 1e-7, `rhw = 1/${d}`);
  assert.ok(Math.abs(flat.z[0] - (10000 / 9999) * (1 - 1 / d)) < 1e-5, 'viewport depth of the z=0 plane');
  // 脚本随后 ins_302 0 回到屏幕模式：像素对齐 ROUND−0.5
  const settled = panel(season + 20);
  assert.equal(settled.x0, 164.5); assert.equal(settled.x1, 474.5); assert.equal(settled.rhw, 1); assert.equal(settled.zw, 0);
});

test('shot screen: cancel returns to Difficulty with cursor popped', needDat, () => {
  const t = MAIN + 1;
  const diff = t + AFTER_MAIN;
  const shot = diff + 2 + AFTER_DIFF;             // DOWN 在 diff，确定在 diff+2
  const out = parse(run(['scene-boot', dat, String(shot + 2 + AFTER_CANCEL + 1), `${t}:${Z},${diff}:${DOWN},${diff + 2}:${Z},${shot}:${RIGHT},${shot + 2}:${X}`]));
  assert.equal(out.header.screen, 5);
  assert.equal(out.header.cursor, 2, 'difficulty cursor restored (Hard)');
  assert.equal(out.header.shot, 1, 'DAT_004a57a4 keeps the shot that was highlighted');
});

test('practice: season confirm goes to Stage select (8); locked stages refuse with SFX 0x10', needDat, () => {
  const t = MAIN + 1;
  const diff = t + 2 + AFTER_MAIN;                // 主菜单 ↓ 到 Practice（2）再确定
  const shot = diff + AFTER_DIFF;
  const season = shot + AFTER_SHOT;
  const stage = season + AFTER_SEASON_PRACTICE;
  const script = [`${t}:${DOWN}`, `${t + 2}:${Z}`, `${diff}:${Z}`, `${shot}:${Z}`, `${season}:${Z}`, `${stage}:${Z}`].join(',');
  const out = parse(run(['scene-boot', dat, String(stage + 2), script]));
  assert.equal(out.header.mode, 1);
  assert.equal(out.header.screen, 8);
  assert.equal(out.header.cursor, 0, 'DAT_004a2974 = -1 clamps to 0');
  assert.deepEqual(out.events.at(-1), ['sfx', '16'], 'no clear data → stage locked');
});

test('stub screens (Option/Replay/…): cancel returns to the main menu', needDat, () => {
  const t = MAIN + 1;
  const downs = Array.from({length: 6}, (_, i) => `${t + i * 2}:${DOWN}`);   // → Option (7)
  const confirmAt = t + 12 + 2;
  const out = parse(run(['scene-boot', dat, String(confirmAt + 21 + 4), [...downs, `${confirmAt}:${Z}`, `${confirmAt + 22}:${X}`].join(',')]));
  assert.equal(out.header.screen, 1);
  assert.ok(out.events.some(e => e[0] === 'stub' && e[1] === '3'));
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
