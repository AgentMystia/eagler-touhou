import { spawn, spawnSync } from 'node:child_process';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { readFileSync } from 'node:fs';

const require = createRequire(import.meta.url);

export function packageExecutable(name, command) {
  const manifest = require.resolve(name + '/package.json');
  const { bin } = JSON.parse(readFileSync(manifest, 'utf8'));
  const path = typeof bin === 'string' ? bin : bin?.[command];
  if (!path) throw new Error(name + ' does not provide ' + command);
  return resolve(dirname(manifest), path);
}

export function findPython(explicit = process.env.PYTHON) {
  for (const command of explicit
    ? [explicit]
    : process.platform === 'win32'
      ? ['python', 'python3']
      : ['python3', 'python']) {
    const result = spawnSync(command, ['--version'], { encoding: 'utf8', windowsHide: true });
    if (result.status === 0) return command;
  }
  throw new Error('需要 Python 3；安装后加入 PATH，或设置 PYTHON。');
}

export function run(command, args, { cwd, env = process.env } = {}) {
  return new Promise((accept, reject) => {
    const child = spawn(command, args, {
      cwd,
      env,
      stdio: 'inherit',
      windowsHide: true,
      shell: false,
    });
    child.once('error', reject);
    child.once('exit', (code, signal) =>
      code === 0 ? accept() : reject(new Error(command + ' 执行失败：' + (signal ?? code))),
    );
  });
}
