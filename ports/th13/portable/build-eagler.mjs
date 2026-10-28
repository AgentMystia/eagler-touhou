import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

for (const script of ['build.mjs', 'package-eagler.mjs']) {
  const result = spawnSync(process.execPath, [fileURLToPath(new URL(script, import.meta.url))], {
    stdio: 'inherit', env: process.env,
  });
  if (result.error) throw result.error;
  if (result.signal || result.status !== 0) throw Error(`${script} failed: ${result.signal || result.status}`);
}
