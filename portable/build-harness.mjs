// th13 双目标构建之一：wasi-sdk 无头 harness（oracle 对照专用）。
// 用法：WASI_SDK_PATH=/opt/wasi-sdk node portable/build-harness.mjs
// 产物：web/artifacts/wasi/th13-harness.wasm（运行方式见 tests/harness/README）
// TH13_DRAW_CAPTURE：语义绘制清单导出开关（draws/draws-replay 模式，
// draw-contract.md v1；web 构建不带此定义，AnmRenderer 钩子编译出局）
import {mkdirSync, readdirSync} from 'node:fs';
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
  ...readdirSync(resolve(root, 'cpp/game')).filter(n => n.endsWith('.cpp')).map(n => 'cpp/game/' + n),
].map(p => resolve(root, p));
const main = resolve(workspace, 'tests/harness/harness.cpp');
const output = resolve(out, 'th13-harness.wasm');

const args = [
  '--target=wasm32-wasip1', '-O2', '-fno-inline-functions', '-std=c++17',
  '-fno-exceptions', '-fno-rtti',
  '-ffp-contract=off', '-DTH13_WASI_HARNESS=1', '-DTH13_DRAW_CAPTURE=1',
  '-I' + resolve(root, 'cpp'),
  ...sources, main, '-o', output,
];
// -fno-inline-functions（2026-10-22 窗B 崩溃遏制，非性能旗语）：
//   症状 = 标题窗B（开局转场期，粒子全量绘制 + 深绘制链）node 原生 SIGSEGV，
//   概率性 5/6，-O0 / --liftoff-only / 粒子不绘制 / 本旗语 四路全 0 崩。
//   机理推定 = O2 跨函数内联膨胀绘制链栈帧（DrawCapture::emit_quad 内联
//   4×append_matrix×16 append_float 等），在 V8 wasm 原生栈（远小于宿主
//   8MB）上溢出且越过 V8 栈检查。根因修复（emit 路径栈瘦身/递归化）属
//   DrawCapture/AnmRenderer 所有权（导出员），修复后可移除本旗语。
//   证据链见 docs/plans/title-anm-blueprint.md §8.7。
console.log('build th13 wasi harness');
const run = spawnSync(clang, args, {stdio: 'inherit'});
if (run.status) throw Error('clang++ failed ' + run.status);
console.log(JSON.stringify({output}, null, 2));
