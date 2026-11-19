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

function structureDigest(ops) {
  return createHash('sha1').update(structureLines(ops).join('\n')).digest('hex');
}

// 跑 harness game-draw，返回 {frame: ops[]}（只保留 golden 里有的帧）
function recordPort(golden, lastFrame, fromFrame) {
  const dir = mkdtempSync(join(tmpdir(), 'th16-draw-'));
  const out = join(dir, 'port.jsonl');
  const c = golden.config;
  try {
    execFileSync(harness, ['game-draw', dat, String(c.stage), String(c.shot), String(c.season), String(lastFrame + 1),
      c.input, '', '', String(c.difficulty), '-1', String(c.lives), String(c.refill ?? 0), '0', out, String(fromFrame)],
    {encoding: 'utf8', maxBuffer: 1 << 28});
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
  const bad = [];
  for (const frame of wanted) {
    assert.ok(port[frame], `port recorded frame ${frame}`);
    if (structureDigest(port[frame]) !== golden.frames[String(frame)]) bad.push(frame);
  }
  assert.deepEqual(bad, [], `${name}: draw structure differs from the original on frames ${bad.slice(0, 10).join(', ')}${bad.length > 10 ? ' …' : ''}`);
  return wanted.length;
}

test('draw structure: stage 1 Reimu / Spring Easy, frames 800–820 and 1000–1500 match the original call stream', needDat, () => {
  assert.equal(checkGolden('draw-st1'), 522);
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
