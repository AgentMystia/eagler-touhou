// 运行 WASI harness（wasm32-wasip1），输出捕获供 oracle 比对。
// 用法：node --experimental-wasi-unstable-preview1 tools/run-harness.mjs [args...]
//   无参数：RNG 冒烟；stage1：装载 st01.ecl 跑 3600 帧，
//   事件流写 web/artifacts/wasi/stage1-events.jsonl（契约见 docs/plans/event-contract.md）。
import {WASI} from 'node:wasi';
import {readFileSync} from 'node:fs';
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
  // 帧数参数（第 4 位）透传：stage1 <资源目录> <输出> [frames]
}
const wasi = new WASI({version: 'preview1', args, env: {}, preopens: {'/': workspace}});
const module = await WebAssembly.compile(bytes);
const instance = await WebAssembly.instantiate(module, wasi.getImportObject());
const code = wasi.start(instance);
process.exit(code ?? 0);
