import { createHash } from 'node:crypto';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { PRODUCT_GAMES } from './contracts/product-catalog.mjs';
import { runtimeFileNames } from './runtime-release.mjs';
import { assertRuntimeDataShell } from './runtime-data-provider.mjs';
import { RUNTIME_VARIANTS, validateRuntimeVariants } from './runtime-variants.mjs';

export function selectRuntimeInputs(game, args, release, releaseRoot) {
  if (release && !release.games[game]) throw new Error(game + ': Runtime Release does not contain this game');
  const entry = {};
  for (const variant of RUNTIME_VARIANTS) {
    const argument = game + (variant.name === 'normal' ? '-build' : '-multiplayer-build');
    const published = release?.games[game]?.[variant.field];
    if (published) entry[variant.field] = { directory: resolve(releaseRoot, published.root), files: published.files };
    else if (!release && args[argument]) entry[variant.field] = { directory: resolve(args[argument]) };
  }
  validateRuntimeVariants(game, entry);
  return entry;
}

/** Copy selected executables once. Game content and feature policy have other owners. */
export async function publishRuntimeVariants({ game, inputs, site }) {
  const urls = {}, directories = {};
  for (const {name, field, suffix, runtime: input} of validateRuntimeVariants(game, inputs)) {
    const directoryLayout = PRODUCT_GAMES[game].runtimeFileLayout === 'directory';
    let declared = input.files;
    if (!declared && directoryLayout) {
      const manifest = JSON.parse(await readFile(resolve(input.directory, 'runtime-files.json'), 'utf8'));
      if (manifest.schema !== 'eagler-touhou/runtime-directory/1') throw new Error(game + ': invalid Runtime directory manifest');
      declared = manifest.files;
    }
    const names = declared ? runtimeFileNames(game, declared) : ['html', 'js', 'wasm'].map(extension => game + '.' + extension);
    const relativeRoot = 'runtime/' + game + suffix;
    const target = resolve(site, relativeRoot), hash = createHash('sha256');
    for (const path of names) {
      const bytes = await readFile(resolve(input.directory, path));
      if (path === game + '.html') assertRuntimeDataShell(bytes.toString('utf8'), game, name);
      if (declared && (bytes.length !== declared[path].bytes ||
          createHash('sha256').update(bytes).digest('hex') !== declared[path].sha256)) {
        throw new Error(game + ' ' + name + ': Runtime identity mismatch: ' + path);
      }
      hash.update(bytes);
      await mkdir(dirname(resolve(target, path)), {recursive: true});
      await writeFile(resolve(target, path), bytes);
    }
    if (directoryLayout) await writeFile(resolve(target, 'runtime-files.json'),
      JSON.stringify({schema: 'eagler-touhou/runtime-directory/1', files: declared}) + '\n');
    directories[field] = target;
    urls[field] = relativeRoot + '/' + game + '.html?hosted=1&v=' + hash.digest('hex').slice(0, 16);
  }
  return {urls, directories};
}
