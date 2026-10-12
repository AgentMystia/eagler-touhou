import { cp, mkdir, mkdtemp, readFile, writeFile, readdir, rm } from 'node:fs/promises';
import { resolve, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';
import { build } from 'esbuild';
import { dependencyExecutable } from '../../lib/source-layout.mjs';
import { HOST_SITE_ARTWORK_FILES } from '../../lib/frontend-static-manifest.mjs';
import { writeRuntimeGeneration, publishRuntimeManifest } from '../../lib/runtime-generations.mjs';
import { validateReactAppShellDeployment } from '../../app/services/app-shell-deployment.ts';
import { buildReactAppShell } from '../../scripts/ui/app-shell-build.ts';
import { writeRuntimeFixture } from './runtime-recovery-fixture.mjs';

const [directory, version = 'a'] = process.argv.slice(2);
if (!directory || !/^[a-z0-9-]+$/.test(version))
  throw new Error('usage: build-pwa-fixture.mjs OUTPUT [VERSION]');
const project = fileURLToPath(new URL('../../', import.meta.url));
const root = resolve(directory);
const origin = process.env.EAGLER_PWA_TEST_ORIGIN;
const site = process.env.EAGLER_PWA_TEST_SITE_ROOT;
if (!origin || !site) throw new Error('PWA fixture requires its loopback origin and site root');
const subdirectory = relative(resolve(site), root).split(sep).join('/');
if (subdirectory === '..' || subdirectory.startsWith('../') || subdirectory.startsWith('/'))
  throw new Error('PWA fixture must remain inside its test site');
if (!['127.0.0.1', 'localhost', '[::1]'].includes(new URL(origin).hostname))
  throw new Error('PWA fixture origin must be loopback');
const deployment = validateReactAppShellDeployment({
  schema: 'eagler-touhou/react-app-shell/1', origin,
  mountPath: subdirectory ? '/' + subdirectory + '/' : '/',
});
await mkdir(root, { recursive: true });
await mkdir(resolve(project, '.cache'), { recursive: true });
const work = await mkdtemp(resolve(project, '.cache/pwa-fixture-'));
try {
  execFileSync(process.execPath, [dependencyExecutable('@react-router/dev', 'react-router'), 'build'], {
    cwd: project, stdio: 'inherit', env: {
      ...process.env, EAGLER_REACT_MOUNT_PATH: deployment.mountPath,
      EAGLER_REACT_BUILD_DIRECTORY: work, EAGLER_REACT_APP_SHELL: 'isolated',
      EAGLER_REACT_APP_SHELL_ORIGIN: deployment.origin,
    },
  });
  const client = resolve(work, 'client');
  const artwork = new Set(HOST_SITE_ARTWORK_FILES.map(path => 'assets/' + path));
  await cp(client, root, { recursive: true, filter: source => {
    const path = relative(client, source).split(sep).join('/');
    return path !== 'app-shell-sw.js' && !artwork.has(path);
  }});
} finally {
  await rm(work, { recursive: true, force: true });
}
const index = resolve(root, 'index.html');
await writeFile(index, (await readFile(index, 'utf8')).replace('</head>',
  '<meta name="pwa-fixture-shell" content="' + version + '"></head>'));
// The driver executes the current Runtime selection module outside the site's
// precache graph. Test instrumentation must never expand publication ownership.
await build({ entryPoints: [resolve(project, 'src/launcher/runtime-launch.mts')],
  outfile: resolve(root, '..', root.split(/[\\/]/).at(-1) + '-driver.js'),
  bundle: true, platform: 'browser', format: 'iife', globalName: '__pwaRuntimeDriver',
  logLevel: 'silent' });
const groups = [];
for (const game of ['pwa-test', 'pwa-unused']) {
  const source = resolve(root, '.tmp', game);
  await rm(source, { recursive: true, force: true });
  await writeRuntimeFixture(source, version);
  const current = await writeRuntimeGeneration({ site: root, root: 'runtime/' + game + '/',
    source, entry: 'runtime.html', names: await readdir(source) });
  groups.push({ root: 'runtime/' + game + '/', current });
  await rm(source, { recursive: true });
}
await publishRuntimeManifest(root, groups);
const result = await buildReactAppShell({
  clientDirectory: root, mountPath: deployment.mountPath, appShell: deployment,
});
if (result.warnings.length) throw new Error(result.warnings.join('\n'));
if (result.manifestEntries.some(entry => !/^[a-f0-9]{64}$/.test(entry.revision || '')))
  throw new Error('precache entry has no content identity');
console.log(JSON.stringify({ build: result.buildId, entries: result.count }));
