// th13 双目标构建之一：wasi-sdk 无头 harness（oracle 对照专用）。
// 用法：WASI_SDK_PATH=/opt/wasi-sdk node portable/build-harness.mjs
// 产物：web/artifacts/wasi/th13-harness.wasm（运行方式见 tests/harness/README）
import {mkdirSync} from 'node:fs';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const workspace = resolve(fileURLToPath(new URL('../', import.meta.url)));
const root = resolve(workspace, 'web');
const out = resolve(root, 'artifacts', 'wasi');
mkdirSync(out, {recursive: true});

const wasi = process.env.WASI_SDK_PATH;
if (!wasi) throw Error('Set WASI_SDK_PATH to a wasi-sdk installation.');
const clang = resolve(wasi, 'bin', 'clang++');

const sources = [
  'cpp/game/Rng.cpp',
].map(p => resolve(root, p));
const main = resolve(workspace, 'tests/harness/harness.cpp');
const output = resolve(out, 'th13-harness.wasm');

const args = [
  '--target=wasm32-wasip1', '-O2', '-std=c++17', '-fno-exceptions', '-fno-rtti',
  '-ffp-contract=off', '-DTH13_WASI_HARNESS=1',
  '-I' + resolve(root, 'cpp'),
  ...sources, main, '-o', output,
];
console.log('build th13 wasi harness');
const run = spawnSync(clang, args, {stdio: 'inherit'});
if (run.status) throw Error('clang++ failed ' + run.status);
console.log(JSON.stringify({output}, null, 2));
