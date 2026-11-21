// 绘制调用流结构对拍：harness `game-draw` 录下的每帧 Clear / 绘制操作，与原版 D3D 调用流（game-oracle.py --draw，
// 经 tools/oracle/draw-compare.py --structure-golden 折叠成每帧 sha1）逐帧比较。
//
// "结构" = 目标 / 视口 / 图元类型与数量 / 顶点格式 / 纹理 / 渲染状态子集，不含顶点坐标与矩阵——那部分由
// draw-compare.py 带 --eps 对拍（原版 x87 与移植版 SSE 单精度在个别顶点上差 1 ulp）；本测试守的是
// "画了哪些东西、按什么顺序、用什么状态"，例如 HUD 文字、得分飘字、Boss 血条这类整条绘制的有无。
// 行口径必须与 draw-compare.py 的 structure_lines 逐字一致。
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {existsSync, readFileSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join, resolve} from 'node:path';

const here = resolve(import.meta.dirname);
const monorepo = resolve(here, '../../..');
const harness = process.env.TH16_HARNESS ?? resolve(here, 'build/harness');
const dat = process.env.TH16_DAT ?? resolve(monorepo, 'games/th16/original/th16.dat');
const needDat = {skip: !existsSync(dat) && 'original th16.dat not present'};

const WORLD_LAYOUTS = new Set(['WorldUv', 'WorldColorUv']);
const STRUCTURE_STATE_KEYS = ['depthTest', 'depthWrite', 'depthCompare', 'blend', 'srcBlend', 'dstBlend', 'blendOp',
  'alphaTest', 'alphaRef', 'alphaCompare', 'fog', 'fogColor', 'textureFactor',
  'colorOp', 'colorArg1', 'colorArg2', 'alphaOp', 'alphaArg1', 'alphaArg2', 'textureTransform',
  'minFilter', 'magFilter', 'addressU', 'addressV', 'cull'];

function structureLines(ops) {
  const lines = [];
  for (const op of ops) {
    if (op.op !== 'clear' && op.op !== 'draw') continue;
    const target = `${op.target[0]},${op.target[1]}`;
    if (op.op === 'clear') {
      const rect = op.rect === null ? '-' : op.rect.join(',');
      lines.push(`C|${target}|${op.flags}|${op.color}|${rect}`);
      continue;
    }
    const world = WORLD_LAYOUTS.has(op.layout);
    const state = [];
    for (const key of STRUCTURE_STATE_KEYS) {
      if (key === 'textureTransform' && !world) continue;
      state.push(String(op.state[key]));
    }
    lines.push(`D|${target}|${op.topology}|${op.layout}|${op.primitives}|${op.texture[0]},${op.texture[1]}|${op.viewport.join(',')}|${state.join(',')}`);
  }
  return lines;
}

// golden.texture_slots 存在时只摘要用这些 AnmFile 槽贴图的绘制操作（draw-compare.py --only-textures 同口径）
function structureDigest(ops, textureSlots) {
  if (textureSlots) ops = ops.filter(op => op.op === 'draw' && textureSlots.has(op.texture[0]));
  return createHash('sha1').update(structureLines(ops).join('\n')).digest('hex');
}

// 跑 harness game-draw，返回 {frame: ops[]}（只保留 golden 里有的帧）
function recordPort(golden, lastFrame, fromFrame) {
  const dir = mkdtempSync(join(tmpdir(), 'th16-draw-'));
  const out = join(dir, 'port.jsonl');
  const c = golden.config;
  try {
    execFileSync(harness, ['game-draw', dat, String(c.stage), String(c.shot), String(c.season), String(lastFrame + 1),
      c.input, '', '', String(c.difficulty), String(c.replay_slot ?? -1), String(c.lives), String(c.refill ?? 0),
      String(c.mode ?? 0), out, String(fromFrame)],
    // 绘制流写进 out；stdout 上的逐帧 game-run JSON 不需要（长序列会超过 maxBuffer），直接丢弃
    {encoding: 'utf8', stdio: ['ignore', 'ignore', 'pipe'], maxBuffer: 1 << 24});
    const frames = {};
    for (const line of readFileSync(out, 'utf8').split('\n')) {
      if (!line) continue;
      const rec = JSON.parse(line);
      if (golden.frames[String(rec.frame)] !== undefined) frames[rec.frame] = rec.ops;
    }
    return frames;
  } finally {
    rmSync(dir, {recursive: true, force: true});
  }
}

function checkGolden(name) {
  const golden = JSON.parse(readFileSync(resolve(here, 'golden', `${name}.json`), 'utf8'));
  const wanted = Object.keys(golden.frames).map(Number).sort((a, b) => a - b);
  const port = recordPort(golden, wanted.at(-1), wanted[0]);
  const textureSlots = golden.texture_slots ? new Set(golden.texture_slots) : null;
  const bad = [];
  for (const frame of wanted) {
    assert.ok(port[frame], `port recorded frame ${frame}`);
    if (structureDigest(port[frame], textureSlots) !== golden.frames[String(frame)]) bad.push(frame);
  }
  assert.deepEqual(bad, [], `${name}: draw structure differs from the original on frames ${bad.slice(0, 10).join(', ')}${bad.length > 10 ? ' …' : ''}`);
  return wanted.length;
}

test('draw structure: stage 1 Reimu / Spring Easy, frames 800–820 and 1000–1500 match the original call stream', needDat, () => {
  assert.equal(checkGolden('draw-st1'), 522);
});

test('draw structure: stage 1 Normal Reimu spring idle, frames 2000–2100 match the original call stream (geometry eps 1e-4, 101 frames)', needDat, () => {
  assert.equal(checkGolden('draw-st1-normal-2000'), 101);
});

test('draw structure: stage 1 Cirno / Autumn Normal, first 8 frames after load (HUD frame, logo, loading text camera switch)', needDat, () => {
  assert.equal(checkGolden('draw-st1-boot'), 8);
});

test('draw structure: stage 1 Reimu / Autumn Normal season release, frames 1030–1110 incl. the "+10" release bonus text (0x40de30)', needDat, () => {
  assert.equal(checkGolden('draw-st1-release'), 81);
});

test('draw structure: stage 1 continue menu Credit line (0x43edc0), frames 1050–1110', needDat, () => {
  assert.equal(checkGolden('draw-st1-menu-credit'), 61);
});

test('draw structure: stage 1 continue → replay slots → name entry (0x43e9b0 / 0x43eb50 / 0x43e730), frames 1120–1180', needDat, () => {
  assert.equal(checkGolden('draw-st1-menu-replay'), 61);
});

test('draw structure: stage 3 Normal spell 26 curve lasers (0x438750 strips via 0x4681f0) + boss distortion grid (0x418c70) + HP ring / effect strips (0x4632f0), frames 230–330', needDat, () => {
  assert.equal(checkGolden('draw-st3-curve-laser'), 101);
});

test('draw structure: stage 1 Normal STD ins 17 stage distortion (0x40c4a0 kind 1 via draw hook 4) + enemy main VMs on layer 8 (0x41b580 anm_layer_offset = 1), frames 4200–4400', needDat, () => {
  assert.equal(checkGolden('draw-st1-distortion'), 201);
});

test('draw structure: stage 1 Easy boss dialogue (draw hook 5 = 0x42b5f0 text follow) + boss life ring markers (0x42865b), frames 6300–6560', needDat, () => {
  assert.equal(checkGolden('draw-st1-dialogue'), 261);
});

test('draw structure: stage 1 Normal Cirno bomb burst particles (draw hook 3 = 0x406860 / 0x469890 line strips), frames 880–1300', needDat, () => {
  assert.equal(checkGolden('draw-st1-cirno-bomb'), 421);
});

test('draw structure: stage 1 Easy first spellcard start / name text / bonus countdown / death during spell, frames 7840–8200', needDat, () => {
  assert.equal(checkGolden('draw-st1-spell'), 361);
});

test('draw structure: stage 1 Easy player death (state 4 → 2 → 0 → 1) and respawn, frames 940–1150', needDat, () => {
  assert.equal(checkGolden('draw-st1-death'), 211);
});

test('draw structure: stage 1 → 2 transition (kind 5 full-screen fade 0x45c860, clear color reset + Loading VM 0x42d200, inherited fog camera 0x4097c0, kind 3 field fade-in 0x45c990), frames 12100–12300', needDat, () => {
  assert.equal(checkGolden('draw-st1-stage-transition'), 201);
});

test('draw structure: stage 4 Normal st04 background (mode 8 texture-matrix cache 0x467b0d) + mode 4 projected leaf particles (0x466390, camera right = facing_norm × up), frames 2–300', needDat, () => {
  assert.equal(checkGolden('draw-st4-leaves'), 299);
});

test('draw structure: stage 2 Easy Reimu spring idle, frames 10–419 match the original call stream (geometry eps 1e-4, 410 frames)', needDat, () => {
  assert.equal(checkGolden('draw-st2'), 410);
});

test('draw structure: stage 2 Easy Reimu spring boss dialogue (0x429ff0 / draw hook 5), frames 7570–7660', needDat, () => {
  assert.equal(checkGolden('draw-st2-dialogue'), 91);
});

test('draw structure: stage 6 Easy Reimu spring idle, frames 10–419 match the original call stream (geometry eps 1e-4, 410 frames)', needDat, () => {
  assert.equal(checkGolden('draw-st6'), 410);
});

test('draw structure: stage 6 Easy Reimu spring idle, frames 2000–2100 match the original call stream (geometry eps 1e-4, 101 frames)', needDat, () => {
  assert.equal(checkGolden('draw-st6-easy-2000'), 101);
});

test('draw structure: stage 3 Easy Reimu spring idle, frames 10–419 match the original call stream (geometry eps 1e-4, 410 frames)', needDat, () => {
  assert.equal(checkGolden('draw-st3'), 410);
});

test('draw structure: stage 3 Easy Reimu spring dialogue (portrait + text box, draw hook 5 = 0x42b5f0), frames 7690–7780 match the original call stream (geometry eps 1e-4, 91 frames)', needDat, () => {
  assert.equal(checkGolden('draw-st3-dialogue'), 91);
});

test('draw structure: stage 3 Normal Reimu spring idle, frames 4000–4100 match the original call stream (geometry eps 1e-4, 101 frames)', needDat, () => {
  assert.equal(checkGolden('draw-st3-normal-4000'), 101);
});

test('draw structure: stage 4 Easy Reimu spring idle, frames 10–419 match the original call stream (geometry eps 1e-4, 410 frames)', needDat, () => {
  assert.equal(checkGolden('draw-st4'), 410);
});

test('draw structure: stage 5 Easy Reimu spring idle, frames 10–419 match the original call stream (geometry eps 1e-4, 410 frames)', needDat, () => {
  assert.equal(checkGolden('draw-st5'), 410);
});

test('draw structure: Extra Reimu doyou idle, frames 10–419 match the original call stream (geometry eps 1e-4, 410 frames)', needDat, () => {
  assert.equal(checkGolden('draw-st7'), 410);
});

test('draw structure: stage 5 Normal spell 46 line lasers (0x433720 beam / tip / origin VMs) + boss distortion grid, frames 400–500', needDat, () => {
  assert.equal(checkGolden('draw-st5-line-laser'), 101);
});

test('draw structure: stage 5 Lunatic Reimu winter midstage line lasers (0x433720 via ECL ins_702, bullet.anm slot 7), frames 3373–3473', needDat, () => {
  assert.equal(checkGolden('draw-st5-mid-laser'), 101);
});

test('draw structure: stage 6 Easy Aya winter BossCard1 line lasers (0x433720 via 0x413860 Ex 0x8000000, bullet.anm slot 7), frames 24645–24740', needDat, () => {
  assert.equal(checkGolden('draw-st6-boss-laser'), 96);
});

test('draw structure: stage 3 Normal Reimu spring BossCard2 curve lasers (0x438750 strip, bullet.anm slot 7), frames 22899–22999', needDat, () => {
  assert.equal(checkGolden('draw-st3-mid-curve'), 101);
});

test('draw structure: Extra spell 115 infinite lasers (0x4357a0 beam / origin VMs), Extra difficulty HUD (0x42cb60), frames 210–330', needDat, () => {
  assert.equal(checkGolden('draw-st7-infinite-laser'), 121);
});

test('draw structure: stage 5 Normal spell 50 curve lasers (0x438750 strips, bullet.anm slot 7, mode 2), frames 395–495', needDat, () => {
  assert.equal(checkGolden('draw-st5-curve-laser'), 101);
});

test('draw structure: Extra spell 117 infinite lasers (0x4357a0, bullet.anm slot 7, mode 2), frames 808–900', needDat, () => {
  assert.equal(checkGolden('draw-st7-card9-laser'), 93);
});

function checkBootGolden(name) {
  const golden = JSON.parse(readFileSync(resolve(here, 'golden', `${name}.json`), 'utf8'));
  const wanted = Object.keys(golden.frames).map(Number).sort((a, b) => a - b);
  const dir = mkdtempSync(join(tmpdir(), 'th16-boot-draw-'));
  const out = join(dir, 'port.jsonl');
  try {
    execFileSync(harness, ['scene-boot', dat, String(wanted.at(-1)), `draw=${out}`, `draw-from=${wanted[0]}`],
      {encoding: 'utf8', stdio: ['ignore', 'ignore', 'pipe']});
    const frames = {};
    for (const line of readFileSync(out, 'utf8').split('\n')) {
      if (!line) continue;
      const rec = JSON.parse(line);
      if (golden.frames[String(rec.frame)] !== undefined) frames[rec.frame] = rec.ops;
    }
    const bad = [];
    for (const frame of wanted) {
      assert.ok(frames[frame], `port recorded boot frame ${frame}`);
      if (structureDigest(frames[frame], null) !== golden.frames[String(frame)]) bad.push(frame);
    }
    assert.deepEqual(bad, []);
    return wanted.length;
  } finally {
    rmSync(dir, {recursive: true, force: true});
  }
}

test('draw structure: boot logo frames 170–180 match the original (0x43b300 ascii.anm 0x11 @ (960,784))', needDat, () => {
  assert.equal(checkBootGolden('draw-boot-170'), 11);
});
