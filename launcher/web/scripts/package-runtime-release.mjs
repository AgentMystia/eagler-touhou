import { RUNTIME_VARIANTS, validateRuntimeVariants } from '../lib/runtime-variants.mjs';
import {normalizeProductSelection} from "../lib/product-selection.mjs";
import { writeRuntimeGeneration } from "../lib/runtime-generations.mjs";
import { createHash, randomUUID } from "node:crypto";
import { cp, mkdir, readFile, rename, rm, writeFile, lstat } from "node:fs/promises";
import { basename, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { PRODUCT_GAMES } from "../lib/contracts/product-catalog.mjs";
import { RUNTIME_RELEASE_SCHEMA, runtimeStem, runtimeFileNames, validateRuntimeReleaseManifest, verifyRuntimeRelease } from "../lib/runtime-release.mjs";
import { extractGameDataLayout } from "../lib/runtime-data-layout.mjs";
import { assertRuntimeDataShell } from "../lib/runtime-data-provider.mjs";

const project = resolve(fileURLToPath(new URL("..", import.meta.url)));
const args = {};
const allowed = new Set(["output", "games", ...Object.keys(PRODUCT_GAMES).flatMap(game =>
  [game + "-build", game + "-multiplayer-build"])]);
for (const value of process.argv.slice(2)) {
  const split = value.indexOf("=");
  if (!value.startsWith("--") || split < 3) throw new Error("invalid argument: " + value);
  const name = value.slice(2, split);
  if (!allowed.has(name) || Object.hasOwn(args, name)) throw new Error("unknown or duplicate argument: --" + name);
  if (!value.slice(split + 1)) throw new Error("empty argument: --" + name);
  args[name] = value.slice(split + 1);
}
const selectedGames = normalizeProductSelection(args.games);
const required = name => {
  if (!args[name]) throw new Error(`missing --${name}=PATH`);
  return resolve(args[name]);
};
const output = required("output");
try {
  await lstat(output);
  throw new Error("Runtime Release output already exists; choose a fresh directory: " + output);
} catch (error) {
  if (error.code !== "ENOENT") throw error;
}
const temporaryRoot = resolve(dirname(output), ".tmp");
const staging = resolve(temporaryRoot, `${basename(output)}.staging-${randomUUID()}`);
const builds = Object.fromEntries(selectedGames.map(game => {
  const entry = {};
  for (const variant of RUNTIME_VARIANTS) {
    const key = game + (variant.name === 'normal' ? '-build' : '-multiplayer-build');
    if (args[key]) entry[variant.field] = resolve(args[key]);
  }
  validateRuntimeVariants(game, entry);
  return [game, entry];
}));
for (const game of Object.keys(PRODUCT_GAMES)) {
  if (!selectedGames.includes(game) && (args[game + '-build'] || args[game + '-multiplayer-build']))
    throw new Error(game + ': build supplied outside --games selection');
}

async function identity(path) {
  const bytes = await readFile(path);
  return { bytes: bytes.length, sha256: createHash("sha256").update(bytes).digest("hex") };
}

async function compileAttestation(build) {
  try {
    return await readFile(resolve(build, "compile_commands.json"), "utf8");
  } catch {
    return "";
  }
}

async function directoryFeatureAttestation(game, build) {
  const manifest = JSON.parse(await readFile(resolve(build, "manifest.json"), "utf8"));
  if (manifest.game !== game || manifest.protocol !== "eagler-touhou/1") {
    throw new Error(`${game}: invalid directory Runtime manifest`);
  }
  return Object.fromEntries(["thprac", "languages", "focusHitbox"].map(feature => {
    const value = manifest.features?.[feature];
    if (value != null && typeof value !== "boolean") {
      throw new Error(`${game}: invalid directory Runtime feature attestation: ${feature}`);
    }
    return [feature, value === true];
  }));
}

async function copyVariant(game, build, root, variant) {
  const stem = runtimeStem(game);
  const html = await readFile(resolve(build, `${stem}.html`), "utf8");
  assertRuntimeDataShell(html, game, variant);
  if (game === "th09" && variant === "multiplayer") {
    const wasm = await readFile(resolve(build, "th09.wasm"));
    const exports = new Set(WebAssembly.Module.exports(new WebAssembly.Module(wasm)).map(entry => entry.name));
    const required = ["th09_peer_url_buffer", "th09_peer_connect", "th09_peer_state", "th09_network_room_begin"];
    const missing = required.filter(name => !exports.has(name));
    if (missing.length) throw new Error(`th09: multiplayer Runtime WASM is missing exports: ${missing.join(", ")}`);
  }
  await mkdir(resolve(staging, root), { recursive: true });
  const files = {};
  if (PRODUCT_GAMES[game].runtimeFileLayout === "directory") {
    const directory = JSON.parse(await readFile(resolve(build, "runtime-files.json"), "utf8"));
    if (directory.schema !== "eagler-touhou/runtime-directory/1") throw new Error(`${game}: missing directory Runtime build manifest`);
    for (const name of runtimeFileNames(game, directory.files)) {
      const source = resolve(build, name), actual = await identity(source), expected = directory.files[name];
      if (actual.bytes !== expected.bytes || actual.sha256 !== expected.sha256) throw new Error(`${game}: stale Runtime build identity: ${name}`);
      await mkdir(dirname(resolve(staging, root, name)), { recursive: true });
      await cp(source, resolve(staging, root, name)); files[name] = actual;
    }
    return files;
  }
  for (const extension of ["html", "js", "wasm"]) {
    const name = `${stem}.${extension}`;
    const source = resolve(build, name);
    await cp(source, resolve(staging, root, name));
    files[name] = await identity(source);
  }
  return files;
}

async function immutableVariant(game, build, root, variant) {
  const files = await copyVariant(game, build, root, variant);
  const generation = await writeRuntimeGeneration({ site: staging, root: root + "/",
    source: resolve(staging, root), entry: `${runtimeStem(game)}.html`,
    names: Object.keys(files), expected: files });
  for (const name of Object.keys(files)) await rm(resolve(staging, root, name));
  return { root: `${root}/${generation.generation}`, generation: generation.generation, files };
}

await mkdir(temporaryRoot, { recursive: true });
await rm(staging, { recursive: true, force: true });
await mkdir(staging, { recursive: true });
try {
const games = {};
for (const game of selectedGames) {
  const product = PRODUCT_GAMES[game];
  const variants = validateRuntimeVariants(game, builds[game]);
  const collected = {};
  let dataLayout = null;
  let layout = null;
  const features = [];
  for (const {name, field, suffix, runtime: build} of variants) {
    collected[field] = await immutableVariant(game, build, 'runtime/' + game + suffix, name);
    let currentLayout = null;
    if (product.dataProvider === 'emscripten-preload') {
      currentLayout = extractGameDataLayout(await readFile(resolve(build, game + '.js'), 'utf8'), game);
      if (layout && (layout.layout !== currentLayout.layout || layout.bytes !== currentLayout.bytes))
        throw new Error(game + ': Runtime variants use different DATA layouts');
      layout ??= currentLayout;
      dataLayout = layout.layout;
    } else {
      const { PRODUCT_CONTENT } = await import('../lib/content-definition.mjs');
      dataLayout = PRODUCT_CONTENT[game]?.dataLayout;
    }
    const directory = product.runtimeFileLayout === 'directory'
      ? await directoryFeatureAttestation(game, build) : null;
    const compile = directory ? '' : await compileAttestation(build);
    const attested = {
      thprac: !!product.features.thprac && (directory?.thprac === true || compile.includes('THPRAC_PORTABLE_ENABLED=1')),
      languages: !!product.features.languages && (directory?.languages === true || compile.includes('-DTH_ENABLE_THCRAP ')),
      focusHitbox: !!product.features.focusHitbox && (directory?.focusHitbox === true ||
        !!currentLayout?.files.some(([path]) => path === '/eagler-hitbox.png')),
    };
    collected[field].features = attested;
    features.push(attested);
  }
  // Site feature controls apply to every published variant of a game. Advertise
  // only capabilities attested by each selected variant; retain individual
  // attestations for independent collection and later combination.
  games[game] = {
    dataProvider: product.dataProvider, dataLayout, ...collected,
    features: Object.fromEntries(Object.keys(product.features).filter(key =>
      ['thprac', 'languages', 'focusHitbox'].includes(key)).map(key =>
      [key, features.every(value => value[key])])),
  };
}

const manifest = validateRuntimeReleaseManifest({
  schema: RUNTIME_RELEASE_SCHEMA,
  protocol: "eagler-touhou/1",
  games,
});
await writeFile(resolve(staging, "runtime-release.json"), `${JSON.stringify(manifest, null, 2)}\n`);
await verifyRuntimeRelease(staging);
await rename(staging, output);
console.log(JSON.stringify({ output, games: Object.keys(games), schema: RUNTIME_RELEASE_SCHEMA }));
} finally {
  await rm(staging, { recursive: true, force: true });
}
