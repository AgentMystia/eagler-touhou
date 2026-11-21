// 标题主菜单对拍：title-oracle.py 直跑原版 0x44ac70 / 0x44af80 / 0x44b530，
// 与 harness scene-boot 比启动幕结束后静止 30 帧的 Ascii 行、一次光标下移的音效与画面号，
// 以及 10 帧 D3D 结构 + 顶点（draw-compare.py --eps 1e-4）。
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {existsSync, mkdtempSync, readFileSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join, resolve} from 'node:path';

const here = resolve(import.meta.dirname);
const monorepo = resolve(here, '../../..');
const harness = process.env.TH16_HARNESS ?? resolve(here, 'build/harness');
const dat = process.env.TH16_DAT ?? resolve(monorepo, 'games/th16/original/th16.dat');
const exe = process.env.TH16_EXE ?? resolve(monorepo, 'games/th16/original/th16.exe');
const datDir = resolve(monorepo, 'artifacts/th16/dat');
const py = process.env.TH16_PYTHON ?? '/tmp/th16-venv/bin/python';
const oracle = resolve(here, '../tools/oracle/title-oracle.py');
const compare = resolve(here, '../tools/oracle/draw-compare.py');
const ready = existsSync(dat) && existsSync(exe) && existsSync(py) && existsSync(datDir);
const needOracle = {skip: !ready && 'th16.exe / th16.dat / title oracle python not present'};

const DOWN = '0x20';
// scene-boot 帧号：第 N 次 logic step 之后。主菜单可输入在第 312 帧（sub==2）。
const MENU_STILL = 312;

function asciiLines(text) {
  return text.split('\n').filter(line => line.startsWith('ascii '));
}

function header(text) {
  const line = text.split('\n').find(l => l.startsWith('scene='));
  return Object.fromEntries(line.split(' ').map(kv => kv.split('=')).map(([k, v]) => [k, Number(v)]));
}

test('title menu: 30 still frames of ascii and one cursor-down match the original', needOracle, () => {
  const dir = mkdtempSync(join(tmpdir(), 'th16-title-'));
  try {
    const staticJson = join(dir, 'static.json');
    execFileSync(py, [oracle, exe, datDir, '341', staticJson], {stdio: ['ignore', 'ignore', 'pipe']});
    const frames = JSON.parse(readFileSync(staticJson, 'utf8')).frames;
    for (let frame = MENU_STILL; frame < MENU_STILL + 30; ++frame) {
      const row = frames[frame - 1];
      assert.equal(row.screen, 1, `oracle screen @${frame}`);
      assert.equal(row.sub, 2, `oracle sub @${frame}`);
      assert.equal(row.cursor, 0, `oracle cursor @${frame}`);
      const port = execFileSync(harness, ['scene-boot', dat, String(frame), 'ascii'], {encoding: 'utf8'});
      assert.deepEqual(asciiLines(port), row.ascii, `ascii @${frame}`);
    }

    const downJson = join(dir, 'down.json');
    execFileSync(py, [oracle, exe, datDir, '317', downJson, '--input', `313:${DOWN}`], {stdio: ['ignore', 'ignore', 'pipe']});
    const moved = JSON.parse(readFileSync(downJson, 'utf8')).frames[313];
    assert.equal(moved.frame, 314);
    assert.equal(moved.screen, 1);
    assert.equal(moved.cursor, 2);
    assert.deepEqual(moved.sfx, [10]);
    const port = execFileSync(harness, ['scene-boot', dat, '317', `313:${DOWN}`], {encoding: 'utf8'});
    const head = header(port);
    assert.equal(head.screen, 1);
    assert.equal(head.sub, 2);
    assert.equal(head.cursor, 2);
    assert.ok(port.split('\n').some(line => line === 'event sfx 10'));
  } finally {
    rmSync(dir, {recursive: true, force: true});
  }
});

test('title menu: 10 draw frames match original structure and vertices', needOracle, () => {
  const dir = mkdtempSync(join(tmpdir(), 'th16-title-draw-'));
  try {
    const state = join(dir, 'state.json');
    const oracleDraw = join(dir, 'oracle.draw.jsonl');
    const portDraw = join(dir, 'port.draw.jsonl');
    execFileSync(py, [oracle, exe, datDir, '321', state, '--draw-out', oracleDraw, '--draw-from', '312'],
                 {stdio: ['ignore', 'ignore', 'pipe']});
    execFileSync(harness, ['scene-boot', dat, '321', `draw=${portDraw}`, 'draw-from=312'], {stdio: ['ignore', 'ignore', 'pipe']});
    execFileSync('python3', [compare, oracleDraw, portDraw, '--frames', '312-321', '--eps', '1e-4'],
                 {stdio: ['ignore', 'pipe', 'pipe']});
  } finally {
    rmSync(dir, {recursive: true, force: true});
  }
});
