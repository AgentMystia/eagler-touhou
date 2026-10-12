import { publishRuntimeDirectory } from './runtime-package.mjs';
import { existsSync } from 'node:fs';
import { mkdir, readFile } from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import { resolve, dirname } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { run, findPython, packageExecutable } from './process.mjs';

export const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const webRoot = resolve(projectRoot, 'launcher/web');
const usage = `工程入口：
  npm run doctor
  npm run dependencies -- [status | init 游戏名...]
  npm run build
  npm run check
  npm run check:workspace
  npm run runtime -- th08 --emsdk PATH --font-root PATH
  npm run runtime -- th08 --variant multiplayer --source PATH --emsdk PATH --font-root PATH
  npm run runtime -- th06 --emsdk PATH --dry-run

可选参数：--jobs N、--python PATH、--cmake PATH、--ninja PATH、--assets PATH、--no-configure、--dry-run。
SDL 构建可用 --build-only 仅编译，--output PATH 指定打包目录；CMake 可用 --source-only 检查无原版资源的构建；--thprac 启用练习界面，\n--imgui-root PATH 覆盖随仓库提供的 ImGui，--runtime-extension PATH 指定可选扩展 CMake 文件。
工具只构建本地文件。推送与部署使用各自明确入口。`;

export function parseArguments(args) {
  const positional = [],
    options = {};
  const flags = new Set([
    'dry-run',
    'no-configure',
    'games',
    'help',
    'build-only',
    'source-only',
    'thprac',
  ]);
  for (let i = 0; i < args.length; i++) {
    const value = args[i];
    if (!value.startsWith('--')) {
      positional.push(value);
      continue;
    }
    const equals = value.indexOf('=');
    const key = value.slice(2, equals < 0 ? undefined : equals);
    if (Object.hasOwn(options, key)) throw new Error('重复参数：--' + key);
    if (flags.has(key) && equals >= 0) throw new Error('--' + key + ' 不接受参数值');
    options[key] = equals >= 0 ? value.slice(equals + 1) : flags.has(key) ? true : args[++i];
    if (
      options[key] === undefined ||
      options[key] === '' ||
      (typeof options[key] === 'string' && options[key].startsWith('--'))
    )
      throw new Error('--' + key + ' 缺少参数值');
  }
  return { positional, options };
}

async function localConfig() {
  try {
    return JSON.parse(await readFile(resolve(projectRoot, 'config/local.json'), 'utf8'));
  } catch (error) {
    if (error.code === 'ENOENT') return {};
    throw error;
  }
}

export async function runtimePlan(game, options = {}) {
  const unknown = Object.keys(options).filter(
    (key) =>
      ![
        'variant',
        'source',
        'output',
        'emsdk',
        'font-root',
        'imgui-root',
        'runtime-extension',
        'jobs',
        'python',
        'dry-run',
        'no-configure',
        'cmake',
        'ninja',
        'assets',
        'build-only',
        'source-only',
        'thprac',
      ].includes(key),
  );
  if (unknown.length) throw new Error('未知构建参数：' + unknown.join(', '));
  const config = JSON.parse(
    await readFile(resolve(projectRoot, 'config/runtime-builds.json'), 'utf8'),
  );
  const entry = config.games[game];
  if (!entry?.sourceBuild) throw new Error('没有登记该游戏的源码构建入口：' + game);
  const variant = options.variant ?? 'normal';
  if (!entry.variants[variant]) throw new Error(game + ' 不支持构建类型：' + variant);
  const local = await localConfig();
  const source = resolve(
    options.source ?? local.sources?.[game]?.[variant] ?? resolve(projectRoot, game),
  );
  if (
    variant === 'multiplayer' &&
    entry.sourceBuild.separateMultiplayerSource &&
    !options.source &&
    !local.sources?.[game]?.[variant]
  )
    throw new Error(game + ' 的多人源码独立维护，请用 --source 指定已有的多人源码目录。');
  if (!existsSync(source)) throw new Error('未初始化源码依赖：' + source);
  const jobs = Number(options.jobs ?? 4);
  if (!Number.isSafeInteger(jobs) || jobs < 1) throw new Error('--jobs 必须是正整数');
  const sdk = options.emsdk ?? process.env.EMSDK ?? local.emsdk;
  if (!sdk && !options['dry-run']) throw new Error('请用 --emsdk 或 EMSDK 指定 Emscripten SDK。');
  const emsdk = sdk ? resolve(sdk) : '<EMSDK>';
  const output = resolve(options.output ?? resolve(projectRoot, 'dist/runtime', game, variant));
  const python = findPython(options.python ?? local.python ?? process.env.PYTHON);
  const fontRoot = options['font-root'] ?? process.env.EAGLER_FONT_ROOT ?? local.fonts?.[game];
  const env = {
    PYTHON: python,
    TH_PYTHON: python,
    EMSDK: emsdk,
    TH09_EMSDK: emsdk,
    TH15_EMSDK: emsdk,
    EAGLER_RUNTIME_OUTPUT: output,
    EAGLER_BUILD_JOBS: String(jobs),
  };
  if (fontRoot) env.EAGLER_FONT_ROOT = resolve(fontRoot);
  const commands = [];
  const feature = entry.variants[variant].features;
  if (options.thprac && feature.thprac === false) throw new Error(game + ' 的该类型未启用练习模式');
  const thprac = options.thprac === true || feature.thprac === true;
  if (entry.sourceBuild.kind !== 'cmake' && (options['imgui-root'] || options['runtime-extension']))
    throw new Error('--imgui-root / --runtime-extension 仅适用于 TH06/TH07 CMake 构建。');
  if (entry.sourceBuild.kind !== 'cmake' && options['source-only'])
    throw new Error('--source-only 仅用于 CMake 构建；SDL 构建用 --build-only。');
  if (entry.sourceBuild.kind !== 'cmake' && options['no-configure'])
    throw new Error('--no-configure 仅用于 CMake 构建。');
  if (entry.sourceBuild.kind === 'cmake') {
    if (options.output) throw new Error('CMake 构建输出位于各作 build/web 目录，不接受 --output。');
    const build = resolve(source, 'build/web', variant);
    const toolchain =
      ['upstream', 'install']
        .map((layout) =>
          resolve(emsdk, layout, 'emscripten/cmake/Modules/Platform/Emscripten.cmake'),
        )
        .find((path) => existsSync(path)) ??
      resolve(emsdk, 'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake');
    const cmake = options.cmake ?? local.cmake ?? 'cmake';
    const ninja = options.ninja ?? local.ninja;
    const cache = {
      ...config.defaults.cache,
      ...entry.variants[variant].cache,
      TH_ENABLE_THCRAP: feature.thcrap !== false,
      TH_ENABLE_THPRAC: thprac,
      CMAKE_TOOLCHAIN_FILE: toolchain,
      CMAKE_EXPORT_COMPILE_COMMANDS: true,
    };
    if (thprac) {
      const imgui = options['imgui-root'] ?? local.imguiRoot ?? resolve(projectRoot, 'third_party/imgui');
      if (!existsSync(resolve(imgui, 'imgui.h')) && !options['dry-run'])
        throw new Error('练习构建的 ImGui 源码不完整：' + imgui);
      cache.THPRAC_IMGUI_ROOT = imgui ? resolve(imgui) : '<IMGUI_ROOT>';
      const extension = options['runtime-extension'] ?? local.runtimeExtension;
      cache.TH_RUNTIME_EXTENSION_CMAKE = extension ? resolve(extension) : '';
    } else cache.TH_RUNTIME_EXTENSION_CMAKE = '';
    if (options['source-only']) cache.TH_EXTERNAL_ASSETS = true;
    const assets = options.assets ?? local.assets?.[game];
    if (assets) cache[entry.assetRootVariable] = resolve(assets);
    if (!cache.TH_EXTERNAL_ASSETS && !assets && !options['dry-run'])
      throw new Error('请用 --assets 指定原版资源目录；只检查源码可加 --source-only。');
    if (ninja) cache.CMAKE_MAKE_PROGRAM = ninja;
    if (!options['no-configure'])
      commands.push({
        command: cmake,
        args: [
          '-S',
          source,
          '-B',
          build,
          '-G',
          'Ninja',
          ...Object.entries(cache).map(
            ([name, value]) =>
              '-D' + name + '=' + (typeof value === 'boolean' ? (value ? 'ON' : 'OFF') : value),
          ),
        ],
      });
    commands.push({ command: cmake, args: ['--build', build, '--parallel', String(jobs)] });
    return {
      game,
      variant,
      source,
      output: build,
      requestedOutput: options.output ? output : null,
      env,
      commands,
    };
  }
  const buildArgs = [...entry.sourceBuild.build];
  if (thprac && entry.sourceBuild.practiceFlag) buildArgs.push(entry.sourceBuild.practiceFlag);
  const packageArgs = [...entry.sourceBuild.package];
  if (game === 'th09' && variant === 'multiplayer') packageArgs.push('--multiplayer');
  if (!options['build-only'] && !fontRoot && !options['dry-run'])
    throw new Error('Runtime 打包需要 --font-root；仅编译可加 --build-only。');
  for (const args of options['build-only'] ? [buildArgs] : [buildArgs, packageArgs]) {
    if (!existsSync(resolve(source, args[0])))
      throw new Error('构建入口不存在：' + resolve(source, args[0]));
    commands.push({
      command: process.execPath,
      args: [resolve(source, args[0]), ...args.slice(1)],
    });
  }
  return {
    game,
    variant,
    source,
    output,
    env,
    commands,
    packageStep: options['build-only'] ? null : commands.length - 1,
  };
}

function git(args, cwd = projectRoot) {
  const result = spawnSync('git', args, { cwd, encoding: 'utf8', windowsHide: true });
  if (result.status !== 0) throw new Error(result.stderr.trim() || 'Git 执行失败');
  return result.stdout.trim();
}

export function dependencyStatus() {
  const lines = git([
    'config',
    '--file',
    '.gitmodules',
    '--get-regexp',
    '^submodule\\..*\\.path$',
  ]).split('\n');
  return lines.map((line) => {
    const path = line.slice(line.indexOf(' ') + 1).trim();
    const expected = git(['ls-files', '--stage', '--', path]).split(/\s+/)[1];
    const installed = existsSync(resolve(projectRoot, path, '.git'));
    const commonRoot = resolve(projectRoot, path, 'third_party/eagler-common');
    const common =
      installed && existsSync(resolve(commonRoot, '.git'))
        ? git(['rev-parse', 'HEAD'], commonRoot)
        : null;
    return {
      path,
      expected,
      installed,
      common,
      actual: installed ? git(['rev-parse', 'HEAD'], resolve(projectRoot, path)) : null,
      changed: installed ? git(['status', '--short'], resolve(projectRoot, path)) : '',
    };
  });
}

async function check(games, options = {}) {
  const local = await localConfig();
  const environment = { ...process.env, PYTHON: findPython(), PYTHONDONTWRITEBYTECODE: '1' };
  await run(process.execPath, [
    '--test',
    '--test-reporter=tap',
    resolve(projectRoot, 'tools/tests/runtime-package.test.mjs'),
  ]);
  await run(process.execPath, [resolve(projectRoot, 'services/netplay/test.mjs')]);
  await run(
    process.execPath,
    [resolve(webRoot, 'scripts/check.mjs'), ...(games ? ['--workspace'] : [])],
    { cwd: webRoot, env: environment },
  );
  const compiler = packageExecutable('typescript', 'tsc');
  for (const config of ['tsconfig.ui.json', 'tsconfig.tools.json'])
    await run(process.execPath, [compiler, '-p', config, '--pretty', 'false'], {
      cwd: webRoot,
      env: environment,
    });
  await run(process.execPath, ['tests/run-ui.mjs'], { cwd: webRoot, env: environment });

  if (games) {
    const cmake = options.cmake ?? local.cmake ?? 'cmake';
    const ctest =
      local.ctest ??
      (cmake === 'cmake'
        ? 'ctest'
        : resolve(dirname(cmake), process.platform === 'win32' ? 'ctest.exe' : 'ctest'));
    const ninja = options.ninja ?? local.ninja,
      compiler = local.cxx;
    for (const [name, source] of [
      ['common', 'eagler-common'],
      ['th06', 'th06/tests'],
      ['th07', 'th07/tests'],
    ]) {
      const build = resolve(projectRoot, '.cache/build', name);
      await run(cmake, [
        '-S', resolve(projectRoot, source), '-B', build, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release',
        ...(name === 'common' ? ['-DEAGLER_COMMON_BUILD_TESTS=ON'] : []),
        ...(ninja ? ['-DCMAKE_MAKE_PROGRAM=' + ninja] : []),
        ...(compiler ? ['-DCMAKE_CXX_COMPILER=' + compiler] : []),
      ]);
      await run(cmake, ['--build', build, '--config', 'Release', '--parallel', '4']);
      await run(ctest, ['--test-dir', build, '-C', 'Release', '--output-on-failure']);
    }
  }
}

export async function main(args = process.argv.slice(2)) {
  const { positional, options } = parseArguments(args);
  const [command, ...rest] = positional;
  if (!command || options.help) {
    console.log(usage);
    return;
  }
  const accepted = { doctor: [], dependencies: [], check: ['games', 'cmake', 'ninja'] }[command];
  if (accepted) {
    const unknown = Object.keys(options).filter((key) => !accepted.includes(key));
    if (unknown.length) throw new Error('未知参数：' + unknown.map((key) => '--' + key).join(', '));
    if (command !== 'dependencies' && rest.length) throw new Error(command + ' 不接受位置参数');
  }
  if (command === 'doctor') {
    const local = await localConfig();
    const [major, minor] = process.versions.node.split('.').map(Number);
    const supported = major >= 26 || (major === 24 && minor >= 15);
    let python = null;
    try {
      python = findPython(local.python);
    } catch {}
    console.log(
      JSON.stringify(
        {
          node: process.version,
          nodeSupported: supported,
          requiredNode: '^24.15.0 || >=26',
          python,
          emsdk: process.env.EMSDK ?? local.emsdk ?? null,
          dependencies: dependencyStatus(),
        },
        null,
        2,
      ),
    );
    if (!supported || !python) process.exitCode = 1;
    return;
  }
  if (command === 'dependencies') {
    if (!rest.length || rest[0] === 'status') {
      console.log(JSON.stringify(dependencyStatus(), null, 2));
      return;
    }
    if (rest[0] !== 'init' || rest.length < 2)
      throw new Error('使用 dependencies init 游戏名... 指定要初始化的依赖。');
    const names = new Set(dependencyStatus().map((row) => row.path));
    for (const name of rest.slice(1))
      if (!names.has(name)) throw new Error('未知 Git 依赖：' + name);
    await run('git', ['submodule', 'update', '--init', '--recursive', '--', ...rest.slice(1)], {
      cwd: projectRoot,
    });
    return;
  }
  if (command === 'runtime') {
    if (rest.length !== 1) throw new Error('runtime 需要一个游戏名');
    const plan = await runtimePlan(rest[0], options);
    if (options['dry-run']) {
      console.log(JSON.stringify(plan, null, 2));
      return;
    }
    if (plan.requestedOutput)
      throw new Error('CMake 构建保留在各作 build/web 目录；使用打包入口导出指定目录。');
    await mkdir(dirname(plan.output), { recursive: true });
    for (const [index, step] of plan.commands.entries()) {
      const invoke = (extra = {}) =>
        run(step.command, step.args, {
          cwd: plan.source,
          env: { ...process.env, ...plan.env, ...extra },
        });
      if (index === plan.packageStep) {
        await publishRuntimeDirectory({
          output: plan.output,
          game: plan.game,
          produce: (directory) => invoke({ EAGLER_RUNTIME_OUTPUT: directory }),
        });
      } else await invoke();
    }
    console.log(JSON.stringify({ game: plan.game, variant: plan.variant, output: plan.output }));
    return;
  }
  if (command === 'check') {
    await check(options.games === true, options);
    return;
  }
  throw new Error('未知命令：' + command + '\n' + usage);
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url)
  main().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
  });
