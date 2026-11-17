// ANM 绘制几何：harness `anm-draw` 把 layer 链上的屏幕四边形（mode 0..3）导出为数值行，
// 与启动幕 sig.anm script0 的稳态期望对照（1280×960 坐标系 ×0.5 → 640×480，pixel snap -0.5）。
// 原版资源不入库：缺少 games/th16/original/th16.dat 时跳过。
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

function rows(script, frames, layer = -1) {
  return execFileSync(harness, ['anm-draw', dat, 'sig.anm', String(script), String(frames), String(layer)], {encoding: 'utf8'})
    .trim().split('\n').filter(Boolean).map(line => {
      const p = line.split(' ');
      return {
        frame: Number(p[0]), layer: Number(p[1]), script: Number(p[2]), id: Number(p[3]), blend: Number(p[4]),
        x0: Number(p[5]), y0: Number(p[6]), x1: Number(p[7]), y1: Number(p[8]),
        x2: Number(p[9]), y2: Number(p[10]), x3: Number(p[11]), y3: Number(p[12]),
        u0: Number(p[13]), v0: Number(p[14]), u1: Number(p[15]), v1: Number(p[16]),
        u2: Number(p[17]), v2: Number(p[18]), u3: Number(p[19]), v3: Number(p[20]),
        color: p[21],
      };
    });
}

test('sig.anm script0: two layer-26 quads cover 640×480 with THTX UVs and fade to white', {skip: !available && 'original th16.dat not present'}, () => {
  const all = rows(0, 15, -1);
  const steady = all.filter(r => r.frame === 15);
  assert.equal(steady.length, 2);
  const byScript = Object.fromEntries(steady.map(r => [r.script, r]));
  // script0：1024×960 @ (0,0) ×0.5 → 512×480，pixel snap -0.5
  assert.equal(byScript[0].layer, 26);
  assert.deepEqual([byScript[0].x0, byScript[0].y0, byScript[0].x1, byScript[0].y3], [-0.5, -0.5, 511.5, 479.5]);
  assert.deepEqual([byScript[0].u0, byScript[0].v0, byScript[0].u1, byScript[0].v2], [0, 0, 1, 0.9375]);
  assert.equal(byScript[0].color, 'ffffffff');
  // script1：256×960 @ (1024,0) ×0.5 → 128×480 @ x=512
  assert.equal(byScript[1].layer, 26);
  assert.deepEqual([byScript[1].x0, byScript[1].y0, byScript[1].x1, byScript[1].y3], [511.5, -0.5, 639.5, 479.5]);
  assert.deepEqual([byScript[1].u0, byScript[1].v0, byScript[1].u1, byScript[1].v2], [0, 0, 1, 0.9375]);
  assert.equal(byScript[1].color, 'ffffffff');
  // 淡入：frame 0 非白，frame 15 纯白（ins_408 10 帧）
  const first = all.filter(r => r.frame === 0);
  assert.equal(first.length, 2);
  assert.notEqual(first[0].color, 'ffffffff');
  assert.match(first[0].color, /^ff[0-9a-f]{6}$/);
});
