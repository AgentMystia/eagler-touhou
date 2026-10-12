import {existsSync, readFileSync} from 'node:fs';
import {createRequire} from 'node:module';
import {dirname, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

export const WEB_ROOT = fileURLToPath(new URL('../', import.meta.url));
export const PROJECT_ROOT = resolve(WEB_ROOT, '../..');

export function contractsSourceDirectory(project = WEB_ROOT) {
  const source = resolve(project, '../../contracts/src');
  return existsSync(resolve(source, 'runtime-protocol.mts')) ? source : null;
}

export function dependencyPath(specifier, project = WEB_ROOT) {
  return createRequire(resolve(project, 'package.json')).resolve(specifier);
}

export function dependencyExecutable(packageName, command, project = WEB_ROOT) {
  const manifest = dependencyPath(packageName + '/package.json', project);
  const {bin} = JSON.parse(readFileSync(manifest, 'utf8'));
  const executable = typeof bin === 'string' ? bin : bin?.[command];
  if (!executable) throw new Error(`Package ${packageName} does not provide ${command}`);
  return resolve(dirname(manifest), executable);
}

/** Source workspace and standalone operator bundles have different roots. */
export function sourceProjectRoot(project = WEB_ROOT) {
  const candidate = resolve(project, '../..');
  if (resolve(candidate, 'launcher/web') !== resolve(project)) return resolve(project);
  try {
    const manifest = JSON.parse(readFileSync(resolve(candidate, 'package.json'), 'utf8'));
    if (Array.isArray(manifest.workspaces) && manifest.workspaces.includes('launcher/web')) return candidate;
  } catch (error) { if (error.code !== 'ENOENT') throw error; }
  return resolve(project);
}
export function projectConfigurationPath(name, project = WEB_ROOT) {
  return resolve(sourceProjectRoot(project), 'config', name);
}
