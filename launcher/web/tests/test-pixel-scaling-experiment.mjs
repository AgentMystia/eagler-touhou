import assert from 'node:assert/strict';
import http from 'node:http';
import { readFile } from 'node:fs/promises';
const { default: puppeteer } = await import(process.env.PIXEL_PUPPETEER || 'puppeteer-core');
const source = await readFile(new URL('../public/pixel-scaling-experiment.mjs', import.meta.url), 'utf8');
const server = http.createServer((req, res) => {
  res.setHeader('content-type', 'text/html');
  if (req.url === '/game') res.end('<style>html,body{margin:0;height:100%;display:grid;place-items:center}canvas{width:1000.5px;height:750.375px}</style><canvas width="640" height="480" tabindex="0"></canvas>');
  else res.end('<section id="fullscreen-stage"><iframe src="/game" style="width:1100px;height:800px;border:0"></iframe></section>');
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const browser = await puppeteer.launch({ executablePath: process.env.PIXEL_BROWSER || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe', headless: true });
try {
  for (const dpr of [1, 1.25, 2]) {
    const page = await browser.newPage();
    await page.setViewport({ width: 1300, height: 950, deviceScaleFactor: dpr });
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await page.waitForFrame(frame => frame.url().endsWith('/game'));
    const frame = page.frames().find(frame => frame.url().endsWith('/game'));
    await frame.waitForSelector('canvas');
    await page.addScriptTag({ content: source, type: 'module' });
    await page.waitForSelector('[data-mode="pixel"]');
    if (dpr === 1) {
      await page.evaluate(() => {
        const stage = document.querySelector('#fullscreen-stage');
        document.querySelector('[data-mode="original"]').addEventListener('click', () => stage.requestFullscreen(), { once: true });
      });
      await page.click('[data-mode="original"]');
      await page.waitForFunction(() => document.fullscreenElement?.contains(document.querySelector('#pixel-scaling-experiment')));
    }
    await page.click('[data-mode="pixel"]');
    assert.equal(await frame.$eval('canvas', c => getComputedStyle(c).imageRendering), 'pixelated');
    await page.click('[data-mode="integer"]');
    const ratios = await frame.$eval('canvas', c => [c.getBoundingClientRect().width * devicePixelRatio / c.width, c.getBoundingClientRect().height * devicePixelRatio / c.height]);
    for (const ratio of ratios) assert.ok(Math.abs(ratio - Math.floor(1000.5 * dpr / 640)) < 0.0001, `DPR ${dpr}: ${ratio}`);
    await page.click('[data-mode="original"]');
    assert.equal(await frame.$eval('canvas', c => c.getBoundingClientRect().width), 1000.5);
    assert.equal(await frame.$eval('canvas', c => c.style.cssText), '');
    await page.click('[data-mode="integer"]');
    await page.click('[data-close]');
    assert.equal(await frame.$eval('canvas', c => c.style.cssText), '');
    await page.close();
  }
  console.log('PASS: same-origin game canvas, fullscreen controls, DPR 1 / 1.25 / 2, mode switching and restoration');
} finally { await browser.close(); server.close(); }
