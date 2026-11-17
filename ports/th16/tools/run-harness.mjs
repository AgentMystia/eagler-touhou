// 运行 WASI harness（wasm32-wasip1）：与 tests/run.sh 的本机 harness 同源同参数。
// 用法：node tools/run-harness.mjs <command> [args...]（路径相对工作区根，如 games/th16/original/th16.dat）
import {WASI} from 'node:wasi';
import {readFileSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

const port = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const workspace = resolve(port, '../..');
const wasmPath = process.env.TH16_HARNESS_WASM ? resolve(process.env.TH16_HARNESS_WASM) : resolve(port, 'web/artifacts/wasi/th16-harness.wasm');
const wasi = new WASI({version: 'preview1', args: ['harness', ...process.argv.slice(2)], env: {}, preopens: {'/': workspace}, returnOnExit: true});
const module = await WebAssembly.compile(readFileSync(wasmPath));
const instance = await WebAssembly.instantiate(module, wasi.getImportObject());
process.exit(wasi.start(instance) ?? 0);
