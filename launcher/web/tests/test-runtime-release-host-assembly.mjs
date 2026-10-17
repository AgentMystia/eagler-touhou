/**
 * 使用显式 Runtime Release 与原作资源检查 Host / External 组装。
 * 输入保持只读；候选、故障注入与输出都位于系统临时目录。
 * 通过空工作区验证组装不会重新打开游戏源码仓库，并检查能力上限、
 * 不可变 Runtime 身份，以及失败转换对上一次输出的保留。
 * 游戏运行与真实设备表现由浏览器 / 游戏检查负责。
 *
 * node tests/test-runtime-release-host-assembly.mjs --runtime-release=DIR
 *   --games=th06 --font=FILE --vanilla-font=FILE --th06-assets=DIR
 * 可按 package-server 的同名参数显式提供 --<game>-data-assets=DIR。
 */
import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { cp, mkdtemp, mkdir, readFile, rm, stat, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';
import { PRODUCT_GAMES } from '../lib/contracts/product-catalog.mjs';
import { normalizeProductSelection } from '../lib/product-selection.mjs';
import { writeRuntimeGeneration } from '../lib/runtime-generations.mjs';
import { verifyRuntimeRelease } from '../lib/runtime-release.mjs';
import { RUNTIME_VARIANTS } from '../lib/runtime-variants.mjs';

const project = fileURLToPath(new URL('..', import.meta.url));
const inputOptions = Object.fromEntries(
  [
    'runtime-release',
    'games',
    'font',
    'vanilla-font',
    ...Object.keys(PRODUCT_GAMES).flatMap((game) => [`${game}-assets`, `${game}-data-assets`]),
  ].map((name) => [name, { type: 'string' }]),
);
const { values: args } = parseArgs({ options: inputOptions, strict: true });

function required(name) {
  if (!args[name])
    throw new Error(`missing --${name}=PATH; Runtime and resource inputs must be explicit`);
  return resolve(args[name]);
}

const inputRelease = required('runtime-release');
const inputManifest = await verifyRuntimeRelease(inputRelease);
const games = normalizeProductSelection(args.games ?? Object.keys(inputManifest.games));
for (const game of games) {
  if (!inputManifest.games[game])
    throw new Error(`${game}: selected game is absent from Runtime Release`);
}
const resources = [
  ['font', required('font')],
  ['vanilla-font', required('vanilla-font')],
  ...games.flatMap((game) => [
    [`${game}-assets`, required(`${game}-assets`)],
    ...(args[`${game}-data-assets`]
      ? [[`${game}-data-assets`, resolve(args[`${game}-data-assets`])]]
      : []),
  ]),
];
for (const [name, path] of resources) {
  const info = await stat(path);
  if (name.endsWith('assets') ? !info.isDirectory() : !info.isFile()) {
    throw new Error(`invalid --${name} resource: ${path}`);
  }
}
for (const game of Object.keys(PRODUCT_GAMES)) {
  if (!games.includes(game) && (args[`${game}-assets`] || args[`${game}-data-assets`])) {
    throw new Error(`resource input supplied for unselected game: ${game}`);
  }
}

function run(commandArgs, env = process.env) {
  return new Promise((resolveRun, reject) => {
    const child = spawn(process.execPath, commandArgs, {
      cwd: project,
      env,
      stdio: 'inherit',
      shell: false,
    });
    child.once('error', reject);
    child.once('exit', (code, signal) =>
      code === 0
        ? resolveRun()
        : reject(new Error(`${commandArgs.join(' ')} failed (${signal || code})`)),
    );
  });
}

function runExpectFailure(commandArgs, env) {
  const child = spawnSync(process.execPath, commandArgs, {
    cwd: project,
    env,
    encoding: 'utf8',
    shell: false,
    timeout: 600_000,
    maxBuffer: 32 * 1024 * 1024,
  });
  assert.equal(child.error, undefined, `${commandArgs.join(' ')} could not complete`);
  assert.notEqual(child.status, 0, `${commandArgs.join(' ')} unexpectedly succeeded`);
  return `${child.stdout || ''}\n${child.stderr || ''}`;
}

const scratch = await mkdtemp(resolve(tmpdir(), 'eagler-runtime-release-host-'));
const runtimeRelease = resolve(scratch, 'runtime-release');
const host = resolve(scratch, 'host');
const external = resolve(scratch, 'external');
const fakeWorkspace = resolve(scratch, 'empty-workspace');
const artwork = resolve(scratch, 'artwork');
const features = resolve(scratch, 'features.json');
const env = { ...process.env, EAGLER_WORKSPACE_ROOT: fakeWorkspace };
const featureConfig = {
  schema: 'eagler-touhou/server-features/1',
  resourceMode: 'hosted',
  netplayRelay: 'wss://relay.example.com/eagler-netplay/',
  gameDataFallback: { url: 'https://downloads.example.com/packages', hint: 'fixture' },
  originMigration: { mode: 'http-to-https' },
  games: Object.fromEntries(
    games.map((game) => [
      game,
      {
        ...(PRODUCT_GAMES[game].features.languages ? { languages: ['ja'] } : {}),
        thprac: false,
      },
    ]),
  ),
};
const resourceArgs = resources.map(([name, path]) => `--${name}=${path}`);
function hostArguments(
  output,
  { profile = 'web-validation-runtime-release', featureConfigPath = features } = {},
) {
  return [
    'scripts/package-server.mjs',
    `--output=${output}`,
    `--runtime-release=${runtimeRelease}`,
    `--games=${games.join(',')}`,
    '--music=midi',
    `--profile=${profile}`,
    `--feature-config=${featureConfigPath}`,
    `--artwork-dir=${artwork}`,
    ...resourceArgs,
  ];
}
function externalArguments() {
  return [
    'scripts/package-external-site.mjs',
    `--source=${host}`,
    `--output=${external}`,
    `--runtime-release=${runtimeRelease}`,
    `--games=${games.join(',')}`,
    '--profile=web-validation-runtime-release-external',
  ];
}

try {
  await Promise.all([mkdir(fakeWorkspace), mkdir(artwork)]);
  await cp(inputRelease, runtimeRelease, { recursive: true, errorOnExist: true });
  const originalRuntimeManifest = await verifyRuntimeRelease(runtimeRelease);
  assert.deepEqual(originalRuntimeManifest, inputManifest);
  const runtimeManifestPath = resolve(runtimeRelease, 'runtime-release.json');
  await writeFile(features, JSON.stringify(featureConfig));
  await run(hostArguments(host), env);
  await run(['scripts/verify-server-build.mjs', host], env);

  const capabilityGame = games.find((game) => PRODUCT_GAMES[game].features.thprac);
  if (capabilityGame) {
    const lowered = structuredClone(originalRuntimeManifest);
    lowered.games[capabilityGame].features.thprac = false;
    for (const { field } of RUNTIME_VARIANTS) {
      if (lowered.games[capabilityGame][field]?.features) {
        lowered.games[capabilityGame][field].features.thprac = false;
      }
    }
    await writeFile(runtimeManifestPath, JSON.stringify(lowered));
    await verifyRuntimeRelease(runtimeRelease);
    const requestedFeatures = structuredClone(featureConfig);
    requestedFeatures.games[capabilityGame].thprac = true;
    const requestedPath = resolve(scratch, 'features-thprac.json');
    await writeFile(requestedPath, JSON.stringify(requestedFeatures));
    const failure = runExpectFailure(
      hostArguments(resolve(scratch, 'host-thprac-mismatch'), { featureConfigPath: requestedPath }),
      env,
    );
    assert.match(
      failure,
      /Runtime Release does not attest requested thprac capability/,
      'Host must reject a requested capability above its Runtime attestation',
    );
    await writeFile(runtimeManifestPath, JSON.stringify(originalRuntimeManifest));
  }

  const releaseFailure = runExpectFailure(
    hostArguments(resolve(scratch, 'release-host'), { profile: 'web-release-hosted' }),
    env,
  );
  assert.match(
    releaseFailure,
    /required host UI asset is missing/,
    'release packaging must report missing artwork for the selected games',
  );

  await run(externalArguments(), env);
  await run(['scripts/verify-server-build.mjs', external], env);
  const provenance = JSON.parse(await readFile(resolve(host, 'release-manifest.json'), 'utf8'));
  assert.deepEqual(Object.keys(provenance.sources), ['launcher/web']);
  const deployment = JSON.parse(await readFile(resolve(host, 'deployment.json'), 'utf8'));
  const hostManifest = JSON.parse(await readFile(resolve(host, 'site-manifest.json'), 'utf8'));
  assert.deepEqual(deployment.games, games);
  assert.equal(hostManifest.shared.netplayRelay, featureConfig.netplayRelay);
  assert.deepEqual(hostManifest.shared.gameDataFallback, featureConfig.gameDataFallback);
  assert.deepEqual(hostManifest.shared.originMigration, featureConfig.originMigration);
  for (const game of games) {
    for (const { field } of RUNTIME_VARIANTS) {
      assert.equal(
        Boolean(hostManifest.games[game][field]),
        Boolean(inputManifest.games[game][field]),
        `${game}: Host must preserve the supplied Runtime variants`,
      );
    }
  }
  const externalDeployment = JSON.parse(
    await readFile(resolve(external, 'deployment.json'), 'utf8'),
  );
  const externalManifest = JSON.parse(
    await readFile(resolve(external, 'site-manifest.json'), 'utf8'),
  );
  const externalCatalog = JSON.parse(
    await readFile(resolve(external, 'release-catalog.json'), 'utf8'),
  );
  assert.equal(externalDeployment.resourceMode, 'external');
  assert.equal(externalManifest.shared.resourceMode, 'external');
  assert.deepEqual(Object.keys(externalCatalog.games).sort(), [...games].sort());
  assert(
    !externalDeployment.files.some(
      (file) => file.path.startsWith('games/') || file.path.startsWith('shared/'),
    ),
    'External retains Package metadata without bundling game/shared payloads',
  );
  for (const game of games) {
    assert(
      externalDeployment.files.some(
        (file) => file.path.startsWith(`runtime/${game}/`) && file.path.endsWith('.wasm'),
      ),
      `${game}: External must retain the selected Runtime code`,
    );
  }

  // 制作另一个有效、闭合的不可变代次，避免仅因损坏 manifest 而得到假阳性。
  const changed = structuredClone(originalRuntimeManifest);
  const changedGame = games[0];
  const changedField = RUNTIME_VARIANTS.find(
    ({ field }) => changed.games[changedGame][field],
  ).field;
  const previous = changed.games[changedGame][changedField];
  const source = resolve(scratch, 'changed-runtime-source');
  await cp(resolve(runtimeRelease, previous.root), source, { recursive: true });
  const wasmName = Object.keys(previous.files).find((name) => name.endsWith('.wasm'));
  assert(wasmName, 'selected Runtime must include a Wasm program');
  const sectionName = Buffer.from('host-assembly-identity');
  const changedWasm = Buffer.concat([
    await readFile(resolve(source, wasmName)),
    Buffer.from([0, sectionName.length + 1, sectionName.length]),
    sectionName,
  ]);
  assert(WebAssembly.validate(changedWasm), 'identity fixture must remain a valid Wasm program');
  await writeFile(resolve(source, wasmName), changedWasm);
  const groupRoot = previous.root.slice(0, previous.root.lastIndexOf('/') + 1);
  const generation = await writeRuntimeGeneration({
    site: runtimeRelease,
    root: groupRoot,
    source,
    entry: `${changedGame}.html`,
    names: Object.keys(previous.files),
  });
  const identities = Object.fromEntries(
    generation.files.map(({ path, ...identity }) => [path, identity]),
  );
  changed.games[changedGame][changedField] = {
    ...previous,
    root: groupRoot + generation.generation,
    generation: generation.generation,
    // 平铺 Runtime 的协议文件顺序由 runtimeFileNames 定义。
    files: Object.fromEntries(Object.keys(previous.files).map((name) => [name, identities[name]])),
  };
  await rm(resolve(runtimeRelease, previous.root), { recursive: true });
  await writeFile(runtimeManifestPath, JSON.stringify(changed));
  await verifyRuntimeRelease(runtimeRelease);
  const deploymentBefore = await readFile(resolve(external, 'deployment.json'));
  const failure = runExpectFailure(externalArguments(), env);
  assert.match(
    failure,
    /external Runtime (?:file set|identity) does not match (?:its )?hosted source/,
  );
  assert.deepEqual(
    await readFile(resolve(external, 'deployment.json')),
    deploymentBefore,
    'failed External conversion must retain its previous inventory',
  );
  for (const file of externalDeployment.files) {
    const bytes = await readFile(resolve(external, file.path));
    assert.equal(bytes.length, file.bytes, `previous External output changed: ${file.path}`);
    assert.equal(
      createHash('sha256').update(bytes).digest('hex'),
      file.sha256,
      `previous External output changed: ${file.path}`,
    );
  }
  assert.deepEqual(
    await verifyRuntimeRelease(inputRelease),
    inputManifest,
    'the explicitly supplied Runtime Release must stay unchanged',
  );
  console.log(
    JSON.stringify({
      runtimeReleaseHost: 'PASS',
      games,
      capabilityRejection: capabilityGame ?? null,
      externalIdentityRejection: true,
      previousOutputPreserved: true,
      sourceRepositories: Object.keys(provenance.sources),
    }),
  );
} finally {
  await rm(scratch, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 });
}
