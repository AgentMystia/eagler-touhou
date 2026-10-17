import { spawn } from 'node:child_process';
import { randomUUID } from 'node:crypto';
import { readFile, rm, stat } from 'node:fs/promises';
import { createServer } from 'node:net';
import { tmpdir } from 'node:os';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';
import { validateSiteManifest } from '../../lib/contracts/site-manifest.mjs';
import { webkitPublicationServerArguments } from '../support/webkit-publication-server.mjs';

const project = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const { values } = parseArgs({
  strict: true,
  options: {
    site: { type: 'string' },
    game: { type: 'string' },
    music: { type: 'string', default: 'none' },
    python: { type: 'string' },
    'package-zip': { type: 'string' },
    'block-game-data': { type: 'boolean', default: false },
    'artifact-dir': { type: 'string' },
  },
});
if (!values.site)
  throw new Error(
    'usage: test-playwright-webkit-gate.mjs --site=HOST_DIR [--game=th06|th07] [--python=EXECUTABLE]',
  );
const output = resolve(values.site);
const diagnostics = resolve(
  values['artifact-dir'] ||
    resolve(tmpdir(), `eagler-playwright-webkit-diagnostics-${randomUUID()}`),
);
const requestedGame = values.game;
const requestedMusic = values.music;
const packageZip = values['package-zip'] ? resolve(values['package-zip']) : null;
const blockGameData = values['block-game-data'];
const python = values.python || process.env.EAGLER_PYTHON || process.env.PYTHON || 'python';

if (requestedGame && !['th06', 'th07'].includes(requestedGame))
  throw new Error(`Unsupported --game value: ${requestedGame}`);
if (!['midi', 'ogg-stream', 'ogg-full', 'none'].includes(requestedMusic))
  throw new Error(`Unsupported --music value: ${requestedMusic}`);
if (packageZip && !requestedGame) throw new Error('--package-zip requires an explicit --game');
if (packageZip && !(await stat(packageZip)).isFile())
  throw new Error('--package-zip must be an existing file');
const manifest = validateSiteManifest(
  JSON.parse(await readFile(resolve(output, 'site-manifest.json'), 'utf8')),
);
const games = requestedGame
  ? [requestedGame]
  : ['th06', 'th07'].filter((game) => manifest.games[game]?.runtime);
if (!games.length || games.some((game) => !manifest.games[game]?.runtime)) {
  throw new Error('Selected Host must publish the normal Runtime for each requested WebKit game');
}

function run(command, args) {
  return new Promise((resolveRun, reject) => {
    const child = spawn(command, args, { cwd: project, stdio: 'inherit', shell: false });
    child.once('error', reject);
    child.once('exit', (code, signal) =>
      code === 0
        ? resolveRun()
        : reject(new Error(`${command} ${args.join(' ')} failed (${signal || code})`)),
    );
  });
}

function freePort() {
  return new Promise((resolvePort, reject) => {
    const server = createServer();
    server.once('error', reject);
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      const port = typeof address === 'object' && address ? address.port : 0;
      server.close((error) => (error ? reject(error) : resolvePort(port)));
    });
  });
}

async function waitForHttp(url, timeoutMs = 15000) {
  const deadline = Date.now() + timeoutMs;
  let lastError = null;
  while (Date.now() < deadline) {
    try {
      const response = await fetch(url, { cache: 'no-store' });
      if (response.ok) return;
      lastError = new Error(`HTTP ${response.status}`);
    } catch (error) {
      lastError = error;
    }
    await new Promise((resolveDelay) => setTimeout(resolveDelay, 100));
  }
  throw new Error(`WebKit smoke server did not become ready: ${lastError || 'timeout'}`);
}

let server = null;
let completed = false;
try {
  // 这里只覆盖所选 Host 的桌面 WebKit 自动化，不代表真实 iPhone / iPad。
  await run(process.execPath, ['scripts/verify-server-build.mjs', output]);
  const port = await freePort();
  server = spawn(process.execPath, webkitPublicationServerArguments(output, port), {
    cwd: project,
    stdio: 'inherit',
    shell: false,
    env: { ...process.env, EAGLER_TOUHOU_HOST: process.env.EAGLER_TOUHOU_HOST || '127.0.0.1' },
  });
  const url = `http://127.0.0.1:${port}/`;
  await waitForHttp(url);
  for (const game of games) {
    const args = [
      'tests/browser/launcher-playwright-webkit.py',
      url,
      game,
      requestedMusic,
      `--artifact-dir=${diagnostics}`,
    ];
    if (packageZip) args.push(`--package-zip=${packageZip}`);
    if (blockGameData) args.push('--block-game-data');
    await run(python, args);
  }
  completed = true;
  console.log(JSON.stringify({ webkitHost: 'PASS', site: output, games, music: requestedMusic }));
} finally {
  if (server && server.exitCode == null && server.signalCode == null) server.kill();
  if (completed) await rm(diagnostics, { recursive: true, force: true });
}
