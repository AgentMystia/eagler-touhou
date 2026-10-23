// th13 网页运行时构建：C++ / SDL3 / Emscripten（对齐 ports/th10 构建约定）。
// 用法：EMSDK=/opt/emsdk node portable/build.mjs [--print-inputs]
import {sourceObjectPath} from '../../../tools/build/source-object.mjs';
import {printingRuntimeInputs, printRuntimeInputs, headerInputs} from '../../../tools/build/runtime-inputs.mjs';
import {compilerArguments} from '../../../tools/build/compiler-arguments.mjs';
import {spawn} from 'node:child_process';
import {readFileSync,writeFileSync,readdirSync,mkdirSync,existsSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';

const builtAt = new Date().toISOString();
const workspace = resolve(fileURLToPath(new URL('../', import.meta.url)));
const game = 'th13';
const root = resolve(workspace, 'web');
const out = resolve(root, 'artifacts', 'sdl3');
if (!printingRuntimeInputs) mkdirSync(out, {recursive: true});

const sdk = process.env.EMSDK ?? (existsSync(resolve(workspace, 'tools/emsdk')) ? resolve(workspace, 'tools/emsdk') : resolve(workspace, '../toolchains/emsdk'));
const emcc = [resolve(sdk, 'install/emscripten/emcc.py'), resolve(sdk, 'upstream/emscripten/emcc.py')].find(existsSync);
if (!emcc && !printingRuntimeInputs) throw Error('Set EMSDK to an installed Emscripten SDK; see docs/adaptation/build.md.');
const env = {...process.env, EM_CONFIG: process.env.EM_CONFIG ?? resolve(sdk, '.emscripten'), EMSDK: sdk, EMCC_CORES: process.env.EMCC_CORES ?? '4'};
const python = process.env.TH_PYTHON ?? 'python';
const run = (args) => new Promise((done, reject) => {
  const p = spawn(python, [emcc, ...compilerArguments(args, {compiler: emcc, output: out})], {cwd: root, env, windowsHide: true, stdio: ['ignore', 'pipe', 'pipe']});
  let log = '';
  p.stdout.on('data', x => { log += x; process.stdout.write(x); });
  p.stderr.on('data', x => { log += x; process.stderr.write(x); });
  p.on('error', reject);
  p.on('exit', code => code ? reject(Error('emcc failed ' + code + '\n' + log)) : done());
});

const common = ['-O2', '-g0', '-fno-strict-aliasing', '-ffp-contract=off',
  '-DTH_SDL3=1', '-DTH_NATIVE_PLATFORM=1', '--use-port=sdl3',
  '-I' + resolve(root, 'cpp')];
const flags = [...common, '-std=c++17', '-fno-exceptions', '-fno-rtti'];

const sources = [
  ...readdirSync(resolve(root, 'cpp/game')).filter(n => n.endsWith('.cpp')).map(n => 'cpp/game/' + n),
  ...readdirSync(resolve(root, 'cpp/platform')).filter(n => n.endsWith('.cpp')).map(n => 'cpp/platform/' + n),
  ...readdirSync(resolve(root, 'cpp/sdl')).filter(n => n.endsWith('.cpp')).map(n => 'cpp/sdl/' + n),
  '../portable/sdl/Renderer.cpp',  // 语义 GLES 渲染后端（与 sdl/ 设备层同链）
];

function headers(dir) {
  return readdirSync(dir, {withFileTypes: true}).flatMap(e => e.isDirectory() ? headers(resolve(dir, e.name)) : /\.(h|hpp|inc)$/.test(e.name) ? [resolve(dir, e.name)] : []);
}
const prefix = JSON.stringify([flags, createHash('sha256').update([...headers(resolve(root, 'cpp'))].sort().map(p => p + readFileSync(p).toString()).join('\n')).digest('hex')]);
await printRuntimeInputs({workspace, root, sources: sources.map(p => resolve(root, p)), headers: headerInputs(resolve(root, 'cpp')), flags});

const objects = resolve(out, 'objects');
if (!printingRuntimeInputs) mkdirSync(objects, {recursive: true});
async function compile(source) {
  const object = sourceObjectPath(objects, workspace, source);
  const key = createHash('sha256').update(prefix).update(readFileSync(source)).digest('hex');
  if (existsSync(object) && existsSync(object + '.key') && readFileSync(object + '.key', 'utf8') === key) return object;
  await run([...flags, '-c', source, '-o', object]);
  writeFileSync(object + '.key', key);
  return object;
}

console.log('Build ' + game + ' C++ / SDL3 / Emscripten');
const buildJobs = Math.max(1, Math.min(16, Number.parseInt(process.env.TH_BUILD_JOBS || '4', 10) || 4));
const outputs = new Array(sources.length);
let next = 0, done = 0;
await Promise.all(Array.from({length: buildJobs}, async () => {
  while (next < sources.length) {
    const i = next++;
    outputs[i] = await compile(resolve(root, sources[i]));
    if (++done % 20 === 0) console.log(done + '/' + sources.length + ' translation units');
  }
}));

const output = resolve(out, game + '-sdl.mjs');
await run([...flags, '--emit-symbol-map', '-sDEFAULT_TO_CXX=1', '--no-entry',
  '-sMODULARIZE=1', '-sEXPORT_ES6=1', '-sENVIRONMENT=web,worker',
  '-sALLOW_MEMORY_GROWTH=1', '-sSTACK_SIZE=1048576', '-sINITIAL_MEMORY=67108864',
  '-sMAXIMUM_MEMORY=1073741824', '-sINVOKE_RUN=0', '-sEXIT_RUNTIME=0',
  '-sMIN_WEBGL_VERSION=2', '-sMAX_WEBGL_VERSION=2', '-sGL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=0',
  '-sEXPORTED_RUNTIME_METHODS=FS,UTF8ToString,HEAP32,HEAPU8,allocateUTF8',  // th13.html 资源进 MEMFS /assets + 错误解码 + 投影/渲染统计读取（HEAPU8 = tests/sdl 统计契约测试）；allocateUTF8 = debug 页传录像名
  ...outputs, '-o', output]);

const wasm = readFileSync(output.replace('.mjs', '.wasm'));
const sha = x => createHash('sha256').update(x).digest('hex');
const sourceFiles = [...sources.map(p => resolve(root, p)), ...headers(resolve(root, 'cpp')), fileURLToPath(import.meta.url)].sort();
const inventory = Object.fromEntries(sourceFiles.map(p => [p.replaceAll('\\', '/'), sha(readFileSync(p))]));
const report = {builtAt, game, kind: 'cpp-sdl3', version: '0.1.0-sdl3-skeleton',
  sources, sourceFiles: inventory, bytes: wasm.length, sha256: sha(wasm),
  loaderSha256: sha(readFileSync(output)),
  toolchain: {emsdkRoot: sdk, layout: 'external'}};
writeFileSync(resolve(out, 'build.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({game, bytes: wasm.length, sha256: report.sha256, output}, null, 2));
