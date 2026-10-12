import { build } from 'esbuild';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';

const root = import.meta.dirname;

/** Produces the executable service closure inside a caller-owned staging directory. */
export async function buildNetplayBundle(output) {
  await mkdir(output, { recursive: true });
  for (const entry of ['netplay-relay.mjs', 'netplay-telemetry.mjs']) {
    await build({
      entryPoints: [resolve(root, entry)],
      outfile: resolve(output, 'server', entry),
      bundle: true, platform: 'node', format: 'esm', target: 'node24',
      external: ['ws'],
    });
  }
  const lock = JSON.parse(await readFile(resolve(root, '../../package-lock.json'), 'utf8'));
  const ws = lock.packages?.['node_modules/ws'];
  if (!ws?.version || !ws.resolved || !ws.integrity) throw new Error('Lockfile does not contain ws');
  const manifest = {
    name: 'eagler-touhou-netplay-relay', version: '0.1.0', private: true, type: 'module',
    engines: { node: '^24.15.0 || >=26' },
    scripts: { start: 'node server/netplay-relay.mjs', telemetry: 'node server/netplay-telemetry.mjs' },
    dependencies: { ws: ws.version },
  };
  const { scripts, ...lockedRoot } = manifest;
  await writeFile(resolve(output, 'package.json'), JSON.stringify(manifest, null, 2) + '\n');
  await writeFile(resolve(output, 'package-lock.json'), JSON.stringify({
    name: manifest.name, version: manifest.version, lockfileVersion: 3, requires: true,
    packages: { '': lockedRoot, 'node_modules/ws': ws },
  }, null, 2) + '\n');
}
