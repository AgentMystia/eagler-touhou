#!/usr/bin/env node
import { existsSync } from 'node:fs';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { resolve } from 'node:path';
import { tmpdir } from 'node:os';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { verifyRuntimeRelease } from '../lib/runtime-release.mjs';
import { normalizeProductSelection } from '../lib/product-selection.mjs';
import { PRODUCT_GAMES } from '../lib/contracts/product-catalog.mjs';
import { prepareSiteResources, resolveHostFrontend } from '../host/lib/site-builder.mjs';
import { ensurePythonEnvironment } from '../host/lib/python-environment.mjs';
import { ensureThtk } from '../host/lib/thtk.mjs';
import { run } from '../host/lib/process.mjs';

const project = fileURLToPath(new URL('../', import.meta.url));
export async function assembleSite(parameters) {
  const games = normalizeProductSelection(parameters.Games);
  for (const key of [
    'RuntimeRelease',
    'FeatureConfig',
    'FontFile',
    'VanillaFontFile',
    'OutputDirectory',
  ])
    if (typeof parameters[key] !== 'string' || !parameters[key])
      throw new Error('Missing assembly input: ' + key);
  const output = resolve(parameters.OutputDirectory);
  if (existsSync(output)) throw new Error('Assembly output already exists: ' + output);
  for (const game of games)
    if (!parameters.GameDirectories?.[game]) throw new Error('Missing GameDirectories.' + game);
  await verifyRuntimeRelease(parameters.RuntimeRelease);
  await resolveHostFrontend({ projectRoot: project });
  const features = JSON.parse(await readFile(parameters.FeatureConfig, 'utf8'));
  if (
    features.schema !== 'eagler-touhou/server-features/1' ||
    (features.resourceMode ?? 'hosted') !== 'hosted'
  )
    throw new Error('Site assembly requires a hosted feature configuration');
  const modes = parameters.Music;
  if (
    !Array.isArray(modes) ||
    !modes.length ||
    new Set(modes).size !== modes.length ||
    modes.some((mode) => !['ogg', 'midi', 'wav'].includes(mode))
  )
    throw new Error('Music must explicitly select supported modes');
  const work = await mkdtemp(resolve(tmpdir(), 'eagler-site-prepare-'));
  try {
    const layout = {
      root: work,
      selectedGames: games,
      games: parameters.GameDirectories,
      music: modes,
      features,
      languagePacks: parameters.LanguagePackDirectories ?? {},
      artworkOverride: parameters.ArtworkDirectory,
    };
    const python = await ensurePythonEnvironment({
      projectRoot: project,
      hostRoot: parameters.PythonEnvironmentDirectory ?? work,
      python:
        parameters.Python ??
        process.env.PYTHON ??
        (process.platform === 'win32' ? 'python' : 'python3'),
    });
    const needsLanguages = games.some(
      (game) =>
        PRODUCT_GAMES[game].features.languages &&
        (features.games?.[game]?.languages ?? ['ja', 'lang_zh-hans', 'lang_en']).some(
          (id) => id !== 'ja',
        ) &&
        !layout.languagePacks[game],
    );
    const thtk = needsLanguages
      ? { ...(await ensureThtk({ hostRoot: parameters.ToolCacheDirectory ?? work })) }
      : {};
    if (parameters.ThtkThanm) thtk.thanm = parameters.ThtkThanm;
    const fonts = { unicode: parameters.FontFile, japanese: parameters.VanillaFontFile };
    const { languages, artwork, dataAssets, ogg } = await prepareSiteResources({
      projectRoot: project,
      layout,
      python,
      fonts,
      thtk,
    });
    const args = [
      resolve(project, 'scripts/package-server.mjs'),
      '--output=' + output,
      '--runtime-release=' + parameters.RuntimeRelease,
      '--font=' + fonts.unicode,
      '--vanilla-font=' + fonts.japanese,
      '--music=' + modes.join(','),
      '--feature-config=' + parameters.FeatureConfig,
      '--artwork-dir=' + artwork,
      '--games=' + games.join(','),
      '--profile=' + (parameters.Profile ?? 'web-validation-package'),
      ...games.map((game) => '--' + game + '-assets=' + layout.games[game]),
      ...Object.entries(dataAssets).map(([game, path]) => '--' + game + '-data-assets=' + path),
      ...Object.entries(languages).map(([game, path]) => '--' + game + '-language-packs=' + path),
      ...Object.entries(ogg ?? {}).map(([game, path]) => '--' + game + '-ogg=' + path),
    ];
    await run(process.execPath, args, { cwd: project });
    await run(process.execPath, [resolve(project, 'scripts/verify-server-build.mjs'), output], {
      cwd: project,
    });
    return output;
  } finally {
    await rm(work, { recursive: true, force: true });
  }
}
if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  const args = process.argv.slice(2);
  if (args.length !== 1 || !args[0].startsWith('--input=') || args[0].length === 8)
    throw new Error('用法：node scripts/assemble-site.mjs --input=PARAMETERS.json');
  const path = resolve(args[0].slice(8));
  const input = JSON.parse(await readFile(path, 'utf8'));
  await assembleSite(input);
}
