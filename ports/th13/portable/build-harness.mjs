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
  '--target=wasm32-wasip1', '-O2', '-std=c++17',
  // wasm 线性内存栈扩至 1MB（wasm-ld 默认 64KB）：窗B 转场期粒子全量
  // 绘制的深调用链（绘制层→draw→quad→material→flush→emit 系）在默认栈
  // 下触底，溢出写入线性内存其他区域后宿主 SIGSEGV，且 V8 分层编译
  // （Liftoff/TurboFan 帧尺寸不同）使症状间歇出现（蓝图 §8.7/§8.9）。
  // 本项与 DrawCapture emit 系 noinline（帧上界）共为根因修复，取代
  // -fno-inline-functions 旗语。
  '-Wl,-z,stack-size=1048576',
  '-fno-exceptions', '-fno-rtti',
  '-ffp-contract=off', '-DTH13_WASI_HARNESS=1', '-DTH13_DRAW_CAPTURE=1',
  '-I' + resolve(root, 'cpp'),
  ...sources, main, '-o', output,
];
// [旗语退场 2026-10-22] 原 -fno-inline-functions 为窗B 崩溃遏制旗语——真根因
//   已定：AnmRegistry 整树 retire 子节点双入 pending_deletion → 双 erase →
//   槽位二次入空闲链 → allocate 复用存活槽 → tick 中 VM 被覆写（难度屏
//   vm(120) "自 spawn" 连锁，蓝图 §8.9）。修复（collect/erase 幂等 + 链扫
//   0x40 跳过）后窗B 10 连跑零崩，旗语移除。历史证据链见蓝图 §8.7/§8.9。

console.log('build th13 wasi harness');
const run = spawnSync(clang, args, {stdio: 'inherit'});
if (run.status) throw Error('clang++ failed ' + run.status);
console.log(JSON.stringify({output}, null, 2));
