import http from 'node:http';
import { readFile, stat } from 'node:fs/promises';
import { createReadStream } from 'node:fs';
import { resolve, sep, extname } from 'node:path';

// Local comparison carrier: reuse an existing site without rebuilding games.
const upstreamPort = Number(process.env.PIXEL_UPSTREAM_PORT || 8080);
const port = Number(process.env.PIXEL_PREVIEW_PORT || 8086);
const experiment = await readFile(new URL('../public/pixel-scaling-experiment.mjs', import.meta.url));
const mounts = Object.entries(JSON.parse(process.env.PIXEL_MOUNTS || '{}'));
const server = http.createServer(async (request, response) => {
  const path = new URL(request.url, 'http://localhost').pathname;
  for (const [prefix, directory] of mounts) {
    if (!path.startsWith(prefix + '/')) continue;
    const root = resolve(directory);
    const file = resolve(root, '.' + decodeURIComponent(path.slice(prefix.length)));
    if (!file.startsWith(root + sep)) { response.writeHead(403); response.end(); return; }
    try {
      const info = await stat(file);
      if (!info.isFile()) throw new Error('Not a file');
      const type = { '.html': 'text/html', '.mjs': 'text/javascript', '.js': 'text/javascript', '.json': 'application/json', '.wasm': 'application/wasm', '.ogg': 'audio/ogg' }[extname(file)] || 'application/octet-stream';
      response.writeHead(200, { 'content-type': type, 'content-length': info.size, 'cache-control': 'no-store' });
      if (request.method === 'HEAD') response.end();
      else createReadStream(file).on('error', () => response.destroy()).pipe(response);
    } catch { response.writeHead(404); response.end(); }
    return;
  }
  if (path === '/__pixel-experiment.mjs') {
    response.writeHead(200, { 'content-type': 'text/javascript', 'cache-control': 'no-store' });
    response.end(experiment);
    return;
  }
  // Do not let an upstream offline worker replace this temporary carrier.
  if (request.headers['service-worker']) { response.writeHead(404); response.end(); return; }
  const proxy = http.request({ hostname: '127.0.0.1', port: upstreamPort, path: request.url,
    method: request.method, headers: { ...request.headers, host: `127.0.0.1:${upstreamPort}`, 'accept-encoding': 'identity' } }, incoming => {
    const headers = { ...incoming.headers, 'cache-control': 'no-store' };
    const inject = request.method === 'GET' && incoming.statusCode === 200 &&
      String(headers['content-type']).includes('text/html') && ['/', '/index.html', '/en.html'].includes(path);
    if (!inject) { response.writeHead(incoming.statusCode, headers); incoming.pipe(response); return; }
    delete headers['content-length'];
    const chunks = [];
    incoming.on('data', chunk => chunks.push(chunk));
    incoming.on('end', () => {
      response.writeHead(incoming.statusCode, headers);
      response.end(Buffer.concat(chunks).toString('utf8') + '<script type="module" src="/__pixel-experiment.mjs"></script>');
    });
  });
  proxy.on('error', () => { response.writeHead(502); response.end('Start the existing local game site first.'); });
  request.pipe(proxy);
});
server.listen(port, '127.0.0.1', () => console.log(`Pixel scaling experiment: http://127.0.0.1:${port} (upstream ${upstreamPort})`));
