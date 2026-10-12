import { existsSync } from "node:fs";
import { stat } from "node:fs/promises";
import { resolve } from "node:path";
import { PRODUCT_GAMES } from "./contracts/product-catalog.mjs";
import { PRODUCT_CONTENT } from "./content-definition.mjs";
import { hostConfigWarnings, readHostConfig } from "./host-config.mjs";
import { verifyRuntimeRelease } from "./runtime-release.mjs";

export const HOST_WORKSPACE_SCHEMA = "eagler-touhou/host-workspace/1";
export const HOST_WORKSPACE_GAMES = Object.freeze(Object.keys(PRODUCT_GAMES));

function normalizeMusic(music) {
  const values = Array.isArray(music) ? music : String(music || "midi,ogg").split(",");
  const modes = [...new Set(values.map(value => String(value).trim().toLowerCase()).filter(Boolean))];
  if (!modes.length) modes.push("midi", "ogg");
  if (modes.some(mode => !["midi", "ogg"].includes(mode))) {
    throw new Error("the self-host workflow supports only midi or midi,ogg; use the site assembly entry for WAV/custom music layouts");
  }
  if (!modes.includes("midi")) modes.unshift("midi");
  return modes;
}

async function requireFile(path, label) {
  let info;
  try { info = await stat(path); } catch { throw new Error(`${label} not found: ${path}`); }
  if (!info.isFile() || info.size <= 0) throw new Error(`${label} is not a non-empty file: ${path}`);
  return path;
}

async function requireDirectory(path, label) {
  let info;
  try { info = await stat(path); } catch { throw new Error(`${label} not found: ${path}`); }
  if (!info.isDirectory()) throw new Error(`${label} is not a directory: ${path}`);
  return path;
}

async function preparedAlternativeReady(gameRoot, alternative) {
  if (!alternative?.directory || !Array.isArray(alternative.markerFiles) || !alternative.markerFiles.length) return false;
  const root = resolve(gameRoot, alternative.directory);
  for (const marker of alternative.markerFiles) {
    let info;
    try { info = await stat(resolve(root, marker)); } catch { return false; }
    if (!info.isFile() || info.size <= 0) return false;
  }
  return true;
}

export function hostWorkspacePaths(root) {
  const hostRoot = resolve(root);
  const gamesRoot = resolve(hostRoot, "games");
  const distRoot = resolve(hostRoot, "dist");
  return Object.freeze({
    schema: HOST_WORKSPACE_SCHEMA,
    root: hostRoot,
    config: resolve(hostRoot, "eagler-touhou.config.json"),
    runtimeRelease: resolve(hostRoot, "runtime-release"),
    dist: distRoot,
    site: resolve(distRoot, "site"),
    importSite: resolve(distRoot, "import-site"),
    importPackages: resolve(distRoot, "import"),
    shared: resolve(hostRoot, "shared"),
    games: Object.freeze(Object.fromEntries(HOST_WORKSPACE_GAMES.map(game => [game, resolve(gamesRoot, game)]))),
  });
}

export async function inspectHostWorkspace(root, { music = ["midi", "ogg"] } = {}) {
  const layout = hostWorkspacePaths(root);
  const modes = normalizeMusic(music);
  const hostConfig = await readHostConfig(layout.config);
  await requireDirectory(layout.runtimeRelease, "Runtime Release directory");
  const runtimeRelease = await verifyRuntimeRelease(layout.runtimeRelease);
  const selectedGames = Object.freeze(Object.keys(runtimeRelease.games));
  const features = { schema: 'eagler-touhou/server-features/1', games: {} };
  for (const game of Object.keys(hostConfig.games))
    if (!selectedGames.includes(game)) throw new Error(game + ': host configuration selects a game absent from Runtime Release');
  for (const game of selectedGames) {
    const supported = runtimeRelease.games[game].features;
    const requested = hostConfig.games[game] ?? {};
    const languages = requested.languages ?? (supported.languages ? ['ja', 'lang_zh-hans', 'lang_en'] : ['ja']);
    const thprac = requested.thprac ?? supported.thprac;
    if (languages.some(id => id !== 'ja') && !supported.languages)
      throw new Error(game + ': Runtime Release does not support language packs');
    if (thprac && !supported.thprac)
      throw new Error(game + ': Runtime Release does not support thprac');
    features.games[game] = Object.freeze({ languages: Object.freeze([...languages]), thprac: !!thprac });
  }
  Object.freeze(features.games);
  Object.freeze(features);
  const artworkOverride = resolve(layout.root, 'artwork');
  const requiresThtk = selectedGames.some(game => features.games[game].languages.some(id => id !== 'ja'));

  for (const game of selectedGames) {
    if (!runtimeRelease.games[game]) throw new Error(`Runtime Release does not contain ${game}`);
    await requireDirectory(layout.games[game], `${game.toUpperCase()} game directory`);
  }

  for (const game of selectedGames) {
    const declaration = PRODUCT_CONTENT[game]?.original;
    if (!declaration) throw new Error(`${game}: original-content input declaration is missing`);
    for (const name of declaration.files || []) {
      await requireFile(resolve(layout.games[game], name), `${game.toUpperCase()} ${name}`);
    }
    if (modes.includes("ogg") &&
        !(await preparedAlternativeReady(layout.games[game], declaration.preparedAlternative))) {
      for (const name of declaration.oggSourceFiles || []) {
        await requireFile(resolve(layout.games[game], name), `${game.toUpperCase()} OGG source ${name}`);
      }
    }
  }

  const bundledUnicode = resolve(layout.shared, "unifont.otf");
  const bundledJapanese = resolve(layout.shared, "japanese-font.otf");
  return Object.freeze({
    ...layout,
    selectedGames,
    features,
    requiresThtk,
    artworkOverride: existsSync(artworkOverride) ? artworkOverride : undefined,
    music: Object.freeze(modes),
    bundledFonts: Object.freeze({
      unicode: existsSync(bundledUnicode) ? bundledUnicode : null,
      japanese: existsSync(bundledJapanese) ? bundledJapanese : null,
    }),
    hostConfig,
    warnings: hostConfigWarnings(hostConfig),
    runtimeReleaseSchema: runtimeRelease.schema,
  });
}
