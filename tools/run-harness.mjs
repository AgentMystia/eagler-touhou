// 运行 WASI harness（wasm32-wasip1），输出捕获供 oracle 比对。
// 用法：node tools/run-harness.mjs [args...]
import {WASI} from 'node:wasi';
import {readFileSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

const port = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const wasmPath = resolve(port, 'web/artifacts/wasi/th13-harness.wasm');
const bytes = readFileSync(wasmPath);
const wasi = new WASI({version: 'preview1', args: ['th13-harness', ...process.argv.slice(2)], env: {}});
const module = await WebAssembly.compile(bytes);
const instance = await WebAssembly.instantiate(module, wasi.getImportObject());
const code = wasi.start(instance);
process.exit(code ?? 0);
