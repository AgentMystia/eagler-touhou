// 运行 WASI harness（wasm32-wasip1），输出捕获供 oracle 比对。
// 用法：node --experimental-wasi-unstable-preview1 tools/run-harness.mjs [args...]
//   无参数：RNG 冒烟；stage1：装载 st01.ecl 跑 3600 帧，
//   事件流写 web/artifacts/wasi/stage1-events.jsonl（契约见 docs/plans/event-contract.md）；
//   replay <rpy> [资源目录] [帧数] [stage 索引]：解码原版录像并逐帧注入
//   （rpy 用工作区根相对路径，如 games/replays/th13_udSr01.rpy）；
//   draws [资源目录] [绘制输出] [帧数] [stage.ecl] / draws-replay <rpy> [资源目录]
//   [绘制输出] [帧数] [stage 索引]：语义绘制清单导出（draw-contract.md v1
//   移植侧；默认输出 artifacts/port-draws/ 下，无输入窗 3600 帧、回放窗帧数显式传）；
//   title [资源目录] [绘制输出] / title-b [资源目录] [绘制输出]：标题窗导出
//   （oracle/title_oracle.py 同窗同规则；窗A 600 帧主菜单→难度、窗B 至开局
//   转场请求帧，boot 装载幕 + 输入时刻表内置）。
import {WASI} from 'node:wasi';
import {readFileSync, mkdirSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

const port = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const workspace = resolve(port, '../../..');
const wasmPath = resolve(port, 'web/artifacts/wasi/th13-harness.wasm');
const bytes = readFileSync(wasmPath);

const args = ['th13-harness', ...process.argv.slice(2)];
// 工作区挂载为 guest 侧 '/'：传给 wasm 的路径用挂载内绝对路径。
if (args[1] === 'stage1') {
  if (!args[2]) args[2] = '/artifacts/reference/raw';
  if (!args[3]) args[3] = '/eagler-touhou/ports/th13/web/artifacts/wasi/stage1-events.jsonl';
  // 位置参数透传：stage1 <资源目录> <输出> [frames] [stage.ecl]
}
if (args[1] === 'replay') {
  // replay <rpy> <资源目录> [帧数] [stage 索引]
  if (!args[2]) throw Error('用法：run-harness.mjs replay <rpy> [资源目录] [帧数] [stage 索引]');
  args[2] = '/' + args[2].replace(/^\/+/, '');
  if (!args[3]) args[3] = '/artifacts/reference/raw';
}
if (args[1] === 'draws') {
  // draws <资源目录> <绘制输出> [帧数] [stage.ecl]
  if (!args[2]) args[2] = '/artifacts/reference/raw';
  if (!args[3]) args[3] = '/artifacts/port-draws/port-draws.jsonl';
}
if (args[1] === 'draws-replay') {
  // draws-replay <rpy> <资源目录> <绘制输出> [帧数] [stage 索引]
  if (!args[2]) throw Error('用法：run-harness.mjs draws-replay <rpy> [资源目录] [绘制输出] [帧数] [stage 索引]');
  args[2] = '/' + args[2].replace(/^\/+/, '');
  if (!args[3]) args[3] = '/artifacts/reference/raw';
  if (!args[4]) args[4] = '/artifacts/port-draws/port-draws-replay.jsonl';
}
if (args[1] === 'title' || args[1] === 'title-b') {
  // title/title-b [资源目录] [绘制输出]（窗长内置：A=600、B=转场请求帧；
  // 输出与 oracle title-draws{,-b}.jsonl 同名同窗）
  if (!args[2]) args[2] = '/artifacts/reference/raw';
  if (!args[3]) args[3] = args[1] === 'title'
      ? '/artifacts/port-draws/title-draws.jsonl'
      : '/artifacts/port-draws/title-draws-b.jsonl';
}
for (const output of [args[3], args[4]]) {
  if (output && output.startsWith('/') && output.endsWith('.jsonl'))
    mkdirSync(dirname(workspace + output), {recursive: true});
}
// TH13_DUMPCHECK / TH13_SHOT_PROBE（可选宿主环境变量）：透传给 wasm——
// 前者 harness 侧 dump 层 2 插值通道字段，后者 StageBattle 出膛/命中探针
// （取证脚手架，见 tests/harness/harness.cpp 与 StageBattle.cpp）。
const env = {};
for (const key of ['TH13_DUMPCHECK', 'TH13_SHOT_PROBE'])
  if (process.env[key]) env[key] = process.env[key];
const wasi = new WASI({version: 'preview1', args, env, preopens: {'/': workspace}});
const module = await WebAssembly.compile(bytes);
const instance = await WebAssembly.instantiate(module, wasi.getImportObject());
const code = wasi.start(instance);
process.exit(code ?? 0);
