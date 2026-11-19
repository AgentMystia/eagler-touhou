// Ending 场景 0xf / Extra 0x10：进出与跳过回标题
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {existsSync} from 'node:fs';
import {resolve} from 'node:path';

const here = resolve(import.meta.dirname);
const monorepo = resolve(here, '../../..');
const harness = process.env.TH16_HARNESS ?? resolve(here, 'build/harness');
const dat = process.env.TH16_DAT ?? resolve(monorepo, 'games/th16/original/th16.dat');
const needDat = {skip: !existsSync(dat) && 'original th16.dat not present'};

function run(args) {
  return execFileSync(harness, args, {encoding: 'utf8'});
}

test('ending-smoke: skip after 0x78 frames returns to title', needDat, () => {
  const out = run(['ending-smoke', dat, '140', '121:0x1']);
  const headerLine = out.trim().split('\n')[0];
  const header = Object.fromEntries(headerLine.split(' ').map(kv => kv.split('=')).map(([k, v]) => {
    const n = Number(v);
    return [k, Number.isFinite(n) && !String(v).includes('/') ? n : v];
  }));
  assert.equal(header.ending_live, 0);
  assert.equal(header.finished, 1);
  assert.equal(header.title_live, 1);
  assert.equal(header.scene, 4);
  assert.equal(header.ending_index, 0, 'shot0 + continues0 → e01');
  assert.ok(out.includes('event begin'));
  assert.ok(out.includes('event msg-loaded'));
  assert.ok(out.includes('event vms'));
  assert.ok(out.includes('event anm-loaded'), 'op7 loads e01.anm');
  assert.ok(out.includes('event bgm'), 'op10 plays ending BGM');
  assert.ok(out.includes('event color'), 'op9 sets text color');
  assert.ok(out.includes('event text'), 'op3 decrypts ending line');
  assert.ok(out.includes('event end'));
  assert.match(headerLine, /bgm=bgm\/th16_14/);
});

test('ending-script: first page reaches wait without confirm', needDat, () => {
  const out = run(['ending-smoke', dat, '5']);
  assert.ok(out.includes('event anm-loaded'));
  assert.ok(out.includes('event anm-script'));
  assert.ok(out.includes('event bgm'));
  assert.ok(out.includes('event text'));
  assert.equal(out.includes('event end'), false, 'still waiting on op5');
  assert.match(out, /ending_live=1/);
});

test('ending-smoke: ExtraEnding goes straight to title', needDat, () => {
  const out = run(['ending-smoke', dat, '5', 'extra=1']);
  const header = Object.fromEntries(out.trim().split('\n')[0].split(' ').map(kv => kv.split('=')).map(([k, v]) => [k, Number(v)]));
  assert.equal(header.ending_live, 0);
  assert.equal(header.title_live, 1);
  assert.equal(header.scene, 4);
});
