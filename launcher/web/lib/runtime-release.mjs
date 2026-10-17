import { validateRuntimeVariants } from './runtime-variants.mjs';
import {normalizeProductSelection} from "./product-selection.mjs";
import { runtimeGenerationId, verifyRuntimeGenerationDirectory } from "./runtime-generations.mjs";
import { RUNTIME_GENERATION_FILE } from "./contracts/runtime-generations.mjs";
import { createHash } from "node:crypto";
import { readdir, readFile, stat } from "node:fs/promises";
import { relative, resolve } from "node:path";
import { PRODUCT_CONTENT } from "./content-definition.mjs";
import { PRODUCT_FEATURE_IDS, PRODUCT_GAMES } from "./contracts/product-catalog.mjs";
import { extractGameDataLayout } from "./runtime-data-layout.mjs";
import { assertRuntimeDataShell } from "./runtime-data-provider.mjs";

export const LEGACY_RUNTIME_RELEASE_SCHEMA = "eagler-touhou/runtime-release/1";
export const IMMUTABLE_RUNTIME_RELEASE_SCHEMA = "eagler-touhou/runtime-release/2";
export const RUNTIME_RELEASE_SCHEMA = "eagler-touhou/runtime-release/3";

const SHA256 = /^[a-f0-9]{64}$/i;
const LAYOUT = /^sha256-[a-f0-9]{64}$/i;

function isCanonicalRelativePath(value) {
  if (typeof value !== "string" || !value || value.startsWith("/") || value.includes("\\") || value.includes(":")) return false;
  const parts = value.split("/");
  return parts.every(part => part && part !== "." && part !== "..");
}

export function runtimeStem(game) {
  return game;
}

// A directory runtime remains a closed, hash-identified code/resource set. It does
// not relax the old Emscripten contract or allow original EXE/DAT/DLL content.
export function runtimeFileNames(game, files) {
  const product = PRODUCT_GAMES[game], stem = runtimeStem(game);
  if (!files || typeof files !== "object" || Array.isArray(files)) throw new Error(`${game}: missing Runtime file identities`);
  const names = Object.keys(files);
  if (product?.runtimeFileLayout !== "directory") {
    const expected = [`${stem}.html`, `${stem}.js`, `${stem}.wasm`];
    if (JSON.stringify(names) !== JSON.stringify(expected)) throw new Error(`${game}: Runtime file set must be HTML/JS/WASM only`);
    return names;
  }
  if (names.length > 2048 || product.runtimeAssets.some(name => !Object.hasOwn(files, name))) throw new Error(`${game}: incomplete directory Runtime`);
  for (const name of names) {
    if (!isCanonicalRelativePath(name) || !(
      name === `${stem}.html` || name === "manifest.json" ||
      /^[a-z0-9._-]+\.mjs$/i.test(name) ||
      /^[a-z0-9._-]+\.css$/i.test(name) ||
      /^[a-z0-9._-]+\.wasm$/i.test(name) ||
      /^[a-z0-9._-]+\.json$/i.test(name) ||
      /^fonts\/[a-z0-9._-]+\.(?:bin|woff2?)$/i.test(name)
    )) throw new Error(`${game}: forbidden directory Runtime file: ${name}`);
  }
  return names;
}

export function validateRuntimeReleaseManifest(manifest) {
  if (!manifest || ![RUNTIME_RELEASE_SCHEMA, IMMUTABLE_RUNTIME_RELEASE_SCHEMA, LEGACY_RUNTIME_RELEASE_SCHEMA].includes(manifest.schema) || manifest.protocol !== "eagler-touhou/1") {
    throw new Error("invalid Runtime Release manifest");
  }
  if (!manifest.games || typeof manifest.games !== "object" || Array.isArray(manifest.games)) {
    throw new Error("Runtime Release games must be a non-empty object");
  }
  const registered = normalizeProductSelection(Object.keys(manifest.games));
  for (const game of registered) {
    const product = PRODUCT_GAMES[game];
    const entry = manifest.games[game];
    if (!entry || entry.dataProvider !== product.dataProvider || !LAYOUT.test(entry.dataLayout || "")) {
      throw new Error(`${game}: invalid Runtime Release data contract`);
    }
    if (manifest.schema !== RUNTIME_RELEASE_SCHEMA &&
        (!entry.runtime || (product.multiplayerRuntime && !entry.multiplayerRuntime))) {
      throw new Error(game + ': legacy Runtime Release requires its original complete variant set');
    }
    const variants = validateRuntimeVariants(game, entry);
    for (const {field: variant, runtime} of variants) {
      if (!runtime || typeof runtime !== "object" || Array.isArray(runtime)) throw new Error(game + ': invalid Runtime variant');
      if (!isCanonicalRelativePath(runtime.root)) {
        throw new Error(`${game} ${variant}: invalid Runtime root`);
      }
      const expectedNames = runtimeFileNames(game, runtime.files);
      if (manifest.schema !== LEGACY_RUNTIME_RELEASE_SCHEMA) {
        const expectedRoot = `runtime/${game}/${variant === "multiplayerRuntime" ? "multiplayer/" : ""}`;
        const files = expectedNames.map(path => ({ path, ...runtime.files[path] }));
        const generation = runtimeGenerationId(`${runtimeStem(game)}.html`, files);
        if (runtime.generation !== generation || runtime.root !== expectedRoot + generation) {
          throw new Error(`${game} ${variant}: invalid immutable Runtime root or generation`);
        }
      }
      for (const name of expectedNames) {
        const file = runtime.files[name];
        if (!file || !Number.isSafeInteger(file.bytes) || file.bytes <= 0 || !SHA256.test(file.sha256 || "")) {
          throw new Error(`${game} ${variant}: invalid identity for ${name}`);
        }
      }
    }
    if (entry.features == null || typeof entry.features !== "object") throw new Error(`${game}: Runtime feature attestation is missing`);
    for (const key of PRODUCT_FEATURE_IDS) {
      if (typeof entry.features[key] !== "boolean") throw new Error(`${game}: invalid Runtime feature ${key}`);
      if (entry.features[key] && !product.features[key]) throw new Error(game + ': unsupported Runtime feature ' + key);
      for (const {runtime} of variants) {
        if (runtime.features == null) continue;
        if (typeof runtime.features[key] !== 'boolean') throw new Error(game + ': invalid variant feature ' + key);
        if (runtime.features[key] && !product.features[key]) throw new Error(game + ': unsupported variant feature ' + key);
        if (entry.features[key] && !runtime.features[key]) throw new Error(game + ': shared feature exceeds a selected variant: ' + key);
      }
    }
  }
  return manifest;
}

export async function verifyRuntimeRelease(root) {
  const base = resolve(root);
  const manifest = validateRuntimeReleaseManifest(JSON.parse(await readFile(resolve(base, "runtime-release.json"), "utf8")));
  const expectedFiles = new Set(["runtime-release.json"]);
  for (const [game, entry] of Object.entries(manifest.games)) {
    const product = PRODUCT_GAMES[game];
    const variants = validateRuntimeVariants(game, entry);
    let normalLayout = null;
    for (const {name: variant, runtime} of variants) {
      const stem = runtimeStem(game);
      if (manifest.schema !== LEGACY_RUNTIME_RELEASE_SCHEMA) {
        const groupRoot = runtime.root.slice(0, runtime.root.lastIndexOf("/") + 1);
        await verifyRuntimeGenerationDirectory(base, groupRoot, {
          generation: runtime.generation, entry: `${stem}.html`,
          files: Object.entries(runtime.files).map(([path, file]) => ({ path, ...file }))
            .sort((a, b) => a.path < b.path ? -1 : a.path > b.path ? 1 : 0),
        });
        expectedFiles.add(`${runtime.root}/${RUNTIME_GENERATION_FILE}`);
      }
      const htmlSource = await readFile(resolve(base, runtime.root, `${stem}.html`), "utf8");
      assertRuntimeDataShell(htmlSource, game, variant);
      for (const [name, identity] of Object.entries(runtime.files)) {
        const rel = `${runtime.root}/${name}`;
        expectedFiles.add(rel);
        const path = resolve(base, runtime.root, name);
        const info = await stat(path);
        if (!info.isFile() || info.size !== identity.bytes) throw new Error(`${game}: Runtime Release size mismatch: ${runtime.root}/${name}`);
        const bytes = await readFile(path);
        const sha256 = createHash("sha256").update(bytes).digest("hex");
        if (sha256 !== identity.sha256) throw new Error(`${game}: Runtime Release hash mismatch: ${runtime.root}/${name}`);
      }
      if (entry.dataProvider === "emscripten-preload") {
        const runtimeScript = await readFile(resolve(base, runtime.root, `${stem}.js`), "utf8");
        const layout = extractGameDataLayout(runtimeScript, game);
        if (layout.layout !== entry.dataLayout) {
          throw new Error(`${game} ${variant}: Runtime Release DATA layout does not match Runtime JS`);
        }
        if (normalLayout && (layout.layout !== normalLayout.layout || layout.bytes !== normalLayout.bytes)) {
          throw new Error(`${game}: normal and multiplayer Runtime DATA layouts differ`);
        }
        normalLayout ??= layout;
      }
    }
    if (entry.dataProvider === "retail-memory") {
      const declaredLayout = PRODUCT_CONTENT[game]?.dataLayout;
      if (!declaredLayout || entry.dataLayout !== declaredLayout) {
        throw new Error(`${game}: Runtime Release DATA layout does not match declared content`);
      }
    }
    if (product.features.focusHitbox && normalLayout) {
      const hasFocusHitbox = normalLayout.files.some(([path]) => path === "/eagler-hitbox.png");
      if (entry.features.focusHitbox !== hasFocusHitbox) {
        throw new Error(`${game}: focusHitbox capability does not match Runtime DATA layout`);
      }
    }
  }

  const actualFiles = new Set();
  async function walk(directory) {
    for (const entry of await readdir(directory, { withFileTypes: true })) {
      const path = resolve(directory, entry.name);
      const rel = relative(base, path).replaceAll("\\", "/");
      if (!isCanonicalRelativePath(rel)) throw new Error(`Runtime Release contains invalid path: ${rel}`);
      if (entry.isSymbolicLink()) throw new Error(`Runtime Release must not contain symlinks: ${rel}`);
      if (entry.isDirectory()) await walk(path);
      else if (entry.isFile()) actualFiles.add(rel);
      else throw new Error(`Runtime Release contains unsupported filesystem entry: ${rel}`);
    }
  }
  await walk(base);
  const unexpected = [...actualFiles].filter(path => !expectedFiles.has(path)).sort();
  const missing = [...expectedFiles].filter(path => !actualFiles.has(path)).sort();
  if (unexpected.length || missing.length) {
    throw new Error(`Runtime Release file set mismatch${unexpected.length ? `; unexpected=${unexpected.join(",")}` : ""}${missing.length ? `; missing=${missing.join(",")}` : ""}`);
  }
  return manifest;
}
