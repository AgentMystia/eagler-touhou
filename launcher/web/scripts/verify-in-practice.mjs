#!/usr/bin/env node
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdir, mkdtemp, readFile, rm, stat, writeFile } from 'node:fs/promises';
import { createSecureServer } from 'node:http2';
import { tmpdir } from 'node:os';
import { dirname, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { brotliCompressSync, constants as zlibConstants } from 'node:zlib';
import lighthouse from 'lighthouse';
import desktopConfig from 'lighthouse/core/config/desktop-config.js';
import { computeMedianRun } from 'lighthouse/core/lib/median-run.js';
import puppeteer from 'puppeteer-core';
import { WebSocketServer } from 'ws';
import { createRoomDirectory } from '../../../services/netplay/room-directory.mjs';
import { buildAppShell } from '../lib/app-shell-build.mjs';
import { APP_SHELL_OUTPUT_FILE } from '../lib/app-shell-policy.mjs';
import { findChromiumExecutable } from '../lib/chromium-executable.mjs';
import {
  FRONTEND_PACKAGE_FILES,
  HOST_SITE_ARTWORK_FILES,
  resolveFrontendPackageSource,
} from '../lib/frontend-manifest.mjs';
import {
  DEFAULT_MULTIPLAYER_PRODUCT_ID,
  DEFAULT_PRODUCT_ID,
  PRODUCT_GAMES,
  PRODUCT_IDS,
  productEnabledForBuild,
} from '../lib/contracts/product-catalog.mjs';
import { SITE_MANIFEST_SCHEMA } from '../lib/contracts/site-manifest.mjs';
import { RELEASE_CATALOG_SCHEMA } from '../lib/contracts/release-catalog.mjs';
import { staticContentCompressible, staticContentType } from '../server/static-content-policy.mjs';

const project = resolve(fileURLToPath(new URL('..', import.meta.url)));
const args = Object.fromEntries(
  process.argv.slice(2).map((value) => {
    const split = value.indexOf('=');
    if (!value.startsWith('--') || split < 3) throw new Error(`invalid argument: ${value}`);
    return [value.slice(2, split), value.slice(split + 1)];
  }),
);
for (const name of Object.keys(args)) {
  if (
    !new Set(['profile', 'report', 'runs', 'min-score', 'openssl', 'tls-key', 'tls-cert']).has(name)
  )
    throw new Error(`unknown argument: --${name}`);
}
// 性能分数默认只作诊断；显式阈值才构成性能门禁。
const minimumScore = args['min-score'] == null ? null : Number(args['min-score']);
if (
  minimumScore != null &&
  (!Number.isInteger(minimumScore) || minimumScore < 0 || minimumScore > 100)
) {
  throw new Error('--min-score must be an integer from 0 to 100');
}
if (Boolean(args['tls-key']) !== Boolean(args['tls-cert'])) {
  throw new Error('--tls-key and --tls-cert must be supplied together');
}
const profile = args.profile || 'reference';
if (!new Set(['reference', 'standard']).has(profile))
  throw new Error('--profile must be reference or standard');
const runCount = Number.parseInt(args.runs || '5', 10);
if (!Number.isInteger(runCount) || runCount < 1 || runCount > 9 || runCount % 2 === 0) {
  throw new Error('--runs must be an odd integer from 1 to 9');
}
const reportPath = resolve(
  args.report || resolve(tmpdir(), 'eagler-touhou-verified-in-practice.json'),
);
if (
  reportPath.toLowerCase() === project.toLowerCase() ||
  reportPath.toLowerCase().startsWith(`${project.toLowerCase()}${sep}`)
) {
  throw new Error(
    'Verified In Practice reports are evidence artifacts and must be written outside the source repository',
  );
}
const referenceConfig = structuredClone(desktopConfig);
if (profile === 'reference') {
  referenceConfig.settings.throttling = {
    ...referenceConfig.settings.throttling,
    rttMs: 10,
    throughputKbps: 40_960,
  };
}

function createReferenceManifest(netplayRelay) {
  const hash = 'a'.repeat(64);
  const layout = 'b'.repeat(64);
  const games = {};
  for (const [game, product] of Object.entries(PRODUCT_GAMES)) {
    if (!productEnabledForBuild(game, false)) continue;
    games[game] = {
      runtime: `runtime/${game}/${game}.html?verified=1`,
      ...(product.multiplayerRuntime
        ? { multiplayerRuntime: `runtime/${game}/multiplayer/${game}.html?verified=1` }
        : {}),
      gameData: {
        path: product.package.dataTarget.slice(1),
        bytes: 1,
        sha256: hash,
        version: `sha256-${hash}`,
        layout: `sha256-${layout}`,
      },
      music: { midi: { files: [] } },
    };
  }
  return {
    schema: SITE_MANIFEST_SCHEMA,
    protocol: 'eagler-touhou/1',
    profile: 'verified-in-practice-reference',
    shared: {
      resourceMode: 'hosted',
      vanillaFont: '__verified__/vanilla-font.ttf',
      unicodeFont: '__verified__/unicode-font.otf',
      netplayRelay,
    },
    games,
  };
}

// 参考场景只测 Launcher。示意图按 catalog 派生，不表示正式素材或游戏加载成本。
const referenceArtworkPaths = new Set(
  Object.values(PRODUCT_GAMES).map((product) => `assets/${product.cardArtwork}`),
);
const referenceArtwork = Buffer.from(
  'UklGRj4AAABXRUJQVlA4IDIAAAAQAwCdASogACAAPp1In0slpCKhqAgAsBOJZwDE2BanFAAA/vOkdd6tpg6o+skYToAAAA==',
  'base64',
);

function createCertificate(directory) {
  if (args['tls-key']) return { key: resolve(args['tls-key']), cert: resolve(args['tls-cert']) };
  const key = resolve(directory, 'localhost-key.pem');
  const cert = resolve(directory, 'localhost-cert.pem');
  const executable = args.openssl || process.env.EAGLER_OPENSSL || 'openssl';
  const result = spawnSync(
    executable,
    [
      'req',
      '-x509',
      '-newkey',
      'rsa:2048',
      '-nodes',
      '-keyout',
      key,
      '-out',
      cert,
      '-days',
      '1',
      '-subj',
      '/CN=localhost',
      '-addext',
      'subjectAltName=DNS:localhost,IP:127.0.0.1',
    ],
    { encoding: 'utf8', windowsHide: true, timeout: 30_000 },
  );
  if (result.status !== 0) {
    throw new Error(
      `Cannot create the localhost TLS certificate with ${executable}: ${result.stderr || result.error || 'unknown error'}. ` +
        'Supply --openssl=EXECUTABLE (or EAGLER_OPENSSL), or --tls-key=FILE --tls-cert=FILE.',
    );
  }
  return { key, cert };
}

async function createReferenceServer(workRoot) {
  const shell = await buildAppShell({ quiet: true, globDirectory: project });
  const certificate = createCertificate(workRoot);
  const metadata = new Map([
    [
      'release-catalog.json',
      Buffer.from(`${JSON.stringify({ schema: RELEASE_CATALOG_SCHEMA, games: {} }, null, 2)}\n`),
    ],
    [APP_SHELL_OUTPUT_FILE, shell.worker],
  ]);
  const server = createSecureServer(
    {
      allowHTTP1: true,
      key: await readFile(certificate.key),
      cert: await readFile(certificate.cert),
    },
    async (request, response) => {
      try {
        const url = new URL(request.url || '/', 'https://localhost');
        const relative = decodeURIComponent(url.pathname.slice(1));
        if (referenceArtworkPaths.has(relative)) {
          const body = referenceArtwork;
          response.writeHead(200, {
            'Content-Type': 'image/webp',
            'Cache-Control': 'public, max-age=31536000, immutable',
            'Content-Length': body.length,
          });
          response.end(request.method === 'HEAD' ? undefined : body);
          return;
        }
        const referenceIcon = relative.startsWith('assets/')
          ? relative.slice('assets/'.length)
          : '';
        if (HOST_SITE_ARTWORK_FILES.includes(referenceIcon) && referenceIcon.endsWith('.png')) {
          const body = await readFile(
            new URL(`../tests/fixtures/reference-artwork/${referenceIcon}`, import.meta.url),
          );
          response.writeHead(200, {
            'Content-Type': 'image/png',
            'Cache-Control': 'public, max-age=31536000, immutable',
            'Content-Length': body.length,
          });
          response.end(request.method === 'HEAD' ? undefined : body);
          return;
        }
        if (relative === 'assets/favicon.ico') {
          const body = Buffer.from(
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><rect width="32" height="32" rx="6" fill="#361317"/><circle cx="16" cy="16" r="8" fill="#a7343e"/></svg>',
          );
          response.writeHead(200, {
            'Content-Type': 'image/svg+xml; charset=utf-8',
            'Cache-Control': 'public, max-age=31536000, immutable',
            'Content-Length': body.length,
          });
          response.end(request.method === 'HEAD' ? undefined : body);
          return;
        }
        const generated = metadata.get(relative);
        if (generated) {
          const type = relative.endsWith('.json')
            ? 'application/json; charset=utf-8'
            : 'text/javascript; charset=utf-8';
          response.writeHead(200, {
            'Content-Type': type,
            'Cache-Control': 'no-store',
            'Content-Length': generated.length,
          });
          response.end(request.method === 'HEAD' ? undefined : generated);
          return;
        }
        const frontendPath = relative || 'index.html';
        if (!FRONTEND_PACKAGE_FILES.includes(frontendPath))
          throw new Error('File is not in the React artifact');
        const file = resolveFrontendPackageSource(frontendPath);
        if (file !== project && !file.startsWith(project + sep))
          throw new Error('path outside project');
        const info = await stat(file);
        if (!info.isFile()) throw new Error('not a file');
        const body = await readFile(file);
        const compress =
          staticContentCompressible(file, body.length) &&
          /\bbr\b/.test(String(request.headers['accept-encoding'] || ''));
        const delivered = compress
          ? brotliCompressSync(body, { params: { [zlibConstants.BROTLI_PARAM_QUALITY]: 5 } })
          : body;
        response.writeHead(200, {
          'Content-Type': staticContentType(file),
          'Cache-Control': 'no-cache',
          ...(compress ? { 'Content-Encoding': 'br', Vary: 'Accept-Encoding' } : {}),
          'Content-Length': delivered.length,
        });
        response.end(request.method === 'HEAD' ? undefined : delivered);
      } catch (error) {
        response.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
        response.end(error instanceof Error ? error.message : 'Not found');
      }
    },
  );
  // 空目录复用正式 Relay 的发现协议；本场景不创建房间或运行游戏。
  const directory = createRoomDirectory({
    rooms: new Map(),
    clearSeat: () => false,
    invalidateReady() {},
    broadcast() {},
    maybeDelete() {},
    releaseTransports() {},
  });
  const sockets = new WebSocketServer({ noServer: true });
  server.on('upgrade', (request, socket, head) => {
    const url = new URL(request.url || '/', 'https://localhost');
    if (url.pathname !== '/netplay') {
      socket.destroy();
      return;
    }
    sockets.handleUpgrade(request, socket, head, (client) =>
      directory.connect(client, url.searchParams.get('member') || ''),
    );
  });
  await new Promise((resolvePromise, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolvePromise);
  });
  const address = server.address();
  assert(address && typeof address === 'object');
  metadata.set(
    'site-manifest.json',
    Buffer.from(JSON.stringify(createReferenceManifest(`wss://localhost:${address.port}/netplay`))),
  );
  return {
    url: `https://localhost:${address.port}/`,
    async close() {
      directory.close();
      for (const socket of sockets.clients) socket.terminate();
      await new Promise((resolvePromise) => sockets.close(resolvePromise));
      await new Promise((resolvePromise) => server.close(resolvePromise));
    },
  };
}

async function runNavigationChecks(browser, url) {
  const checks = [];
  async function scenario(name, action) {
    const page = await browser.newPage();
    const errors = [];
    page.on('console', (message) => {
      if (message.type() === 'error')
        errors.push(`${message.text()} (${message.location().url || 'console'})`);
    });
    page.on('pageerror', (error) => errors.push(error.message));
    try {
      await page.evaluateOnNewDocument(() => {
        localStorage.clear();
        localStorage.setItem('eagler-touhou-first-use-notice-seen-v1', '1');
      });
      await page.goto(url, { waitUntil: 'networkidle0', timeout: 30_000 });
      await page.waitForSelector('.game:not([hidden])', { visible: true });
      await page.waitForFunction(() => window.__eaglerBoot?.done === true);
      await action(page);
      assert.deepEqual(errors, [], `${name} browser errors:\n${errors.join('\n')}`);
      checks.push({ name, pass: true });
    } catch (error) {
      const state = await page
        .evaluate(() => ({
          url: location.href,
          selection: [...document.querySelectorAll('.game.selected')].map((element) => ({
            game: element.getAttribute('data-game'),
            product: element.getAttribute('data-product'),
            current: element.getAttribute('aria-current'),
          })),
          dialogs: [...document.querySelectorAll('dialog[open]')].map((element) => element.id),
          actions: [...document.querySelectorAll('button')]
            .filter((element) => {
              const rect = element.getBoundingClientRect();
              return rect.width > 0 && rect.height > 0;
            })
            .map((element) => ({ id: element.id, text: element.innerText })),
        }))
        .catch(() => ({ url: page.url() }));
      throw new Error(`${name}: ${JSON.stringify({ state, errors })}`, { cause: error });
    } finally {
      await page.close();
    }
  }

  await scenario('catalog-discovery', async (page) => {
    const products = await page.$$eval('.game:not([hidden])', (elements) =>
      elements.map(
        (element) => element.getAttribute('data-product') || element.getAttribute('data-game'),
      ),
    );
    assert.deepEqual(
      products.sort(),
      PRODUCT_IDS.filter((product) => productEnabledForBuild(product, false)).sort(),
      'catalog must expose the products enabled for this build',
    );
    assert.equal(await page.$('#eaglerBootEmergency'), null);
  });

  await scenario('single-player-flow', async (page) => {
    const clicked = await page.$$eval(
      '.game:not([hidden])',
      (elements, product) => {
        const target = elements.find(
          (element) =>
            element.getAttribute('data-game') === product && !element.hasAttribute('data-product'),
        );
        target?.click();
        return Boolean(target);
      },
      DEFAULT_PRODUCT_ID,
    );
    assert(clicked);
    await page.waitForFunction(
      () => document.querySelector('.game.selected')?.getAttribute('aria-current') === 'page',
    );
    const actions = await page.$$eval('button', (elements) =>
      elements
        .filter((element) => {
          const rect = element.getBoundingClientRect();
          return rect.width > 0 && rect.height > 0;
        })
        .map((element) => element.innerText),
    );
    assert(
      actions.some((value) => /启动游戏|Launch Game/i.test(value)),
      'single-player launch action is not discoverable',
    );
  });

  await scenario('multiplayer-flow', async (page) => {
    await page.click(`.game[data-product="${DEFAULT_MULTIPLAYER_PRODUCT_ID}"]`);
    await page.waitForFunction(
      (product) =>
        location.pathname.endsWith('/lobby.html') &&
        new URL(location.href).searchParams.get('game') === product,
      {},
      DEFAULT_MULTIPLAYER_PRODUCT_ID,
    );
    await page.waitForSelector('#createButton:not([disabled])', { visible: true });
    await page.waitForSelector('#codeButton:not([disabled])', { visible: true });
    await page.click('#codeButton');
    await page.waitForSelector('#roomDialog[open] #roomCodeInput', { visible: true });
    assert.equal(
      await page.$eval('#gameSelect', (element) => element.value),
      DEFAULT_MULTIPLAYER_PRODUCT_ID,
      'join form must keep the product selected from the catalog',
    );
  });

  return checks;
}

const workRoot = await mkdtemp(resolve(tmpdir(), 'eagler-verified-in-practice-'));
let closeServer;
let browser;
try {
  const reference = await createReferenceServer(workRoot);
  closeServer = reference.close;
  const chromePath = await findChromiumExecutable();
  browser = await puppeteer.launch({
    executablePath: chromePath,
    headless: true,
    userDataDir: resolve(workRoot, 'chrome-profile'),
    args: [
      '--ignore-certificate-errors',
      '--disable-extensions',
      '--no-first-run',
      '--no-default-browser-check',
    ],
  });
  const navigationChecks = await runNavigationChecks(browser, reference.url);
  const endpoint = new URL(browser.wsEndpoint());
  const results = [];
  for (let attempt = 1; attempt <= runCount; attempt += 1) {
    const result = await lighthouse(
      reference.url,
      {
        port: Number(endpoint.port),
        logLevel: 'silent',
        output: 'json',
        onlyCategories: ['performance', 'accessibility', 'best-practices', 'seo'],
      },
      referenceConfig,
    );
    if (!result) throw new Error(`Lighthouse produced no result for run ${attempt}`);
    results.push(result.lhr);
  }
  const representative = computeMedianRun(results);
  await mkdir(dirname(reportPath), { recursive: true });
  await writeFile(reportPath, `${JSON.stringify(representative, null, 2)}\n`);
  const categories = Object.fromEntries(
    Object.entries(representative.categories).map(([id, category]) => [
      id,
      Math.round(category.score * 100),
    ]),
  );
  const metricIds = [
    'first-contentful-paint',
    'largest-contentful-paint',
    'speed-index',
    'total-blocking-time',
    'cumulative-layout-shift',
  ];
  const metrics = Object.fromEntries(
    metricIds.map((id) => [
      id,
      {
        score: Math.round((representative.audits[id].score ?? 0) * 100),
        value: representative.audits[id].displayValue || '',
      },
    ]),
  );
  const throttling = referenceConfig.settings.throttling;
  const summary = {
    launcherReference: 'PASS',
    scope: 'Synthetic Launcher catalog/navigation; no Runtime execution or game-data transfer',
    minimumScore,
    profile,
    runs: results.map((run, index) => ({
      attempt: index + 1,
      performance: Math.round(run.categories.performance.score * 100),
      totalBlockingTimeMs: Math.round(run.audits['total-blocking-time'].numericValue || 0),
    })),
    network: { rttMs: throttling.rttMs, throughputKbps: throttling.throughputKbps },
    categories,
    metrics,
    navigationChecks,
    report: reportPath,
  };
  const failures =
    minimumScore == null
      ? []
      : [
          ...Object.entries(categories)
            .filter(([, score]) => score < minimumScore)
            .map(([id, score]) => `${id}=${score}`),
          ...Object.entries(metrics)
            .filter(([, value]) => value.score < minimumScore)
            .map(([id, value]) => `${id}=${value.score}`),
        ];
  if (failures.length) {
    summary.launcherReference = 'FAIL';
    throw new Error(
      `Launcher reference score threshold failed: ${failures.join(', ')}\n${JSON.stringify(summary, null, 2)}`,
    );
  }
  console.log(JSON.stringify(summary, null, 2));
} finally {
  if (browser) await browser.close().catch(() => {});
  await closeServer?.();
  await rm(workRoot, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 }).catch(
    () => {},
  );
}
