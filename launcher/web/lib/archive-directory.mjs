import { randomUUID } from 'node:crypto';
import { lstat, mkdir, rename, rm } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { run } from '../host/lib/process.mjs';

export async function archiveDirectory(
  source,
  output,
  {
    prefix = '',
    python = process.env.PYTHON ?? (process.platform === 'win32' ? 'python' : 'python3'),
  } = {},
) {
  const target = resolve(output);
  try {
    await lstat(target);
    throw new Error('Archive already exists: ' + target);
  } catch (error) {
    if (error.code !== 'ENOENT') throw error;
  }
  await mkdir(dirname(target), { recursive: true });
  const temporary = target + '.incomplete-' + randomUUID();
  try {
    await run(python, [
      fileURLToPath(new URL('../scripts/create-zip.py', import.meta.url)),
      resolve(source),
      temporary,
      '--prefix',
      prefix,
    ]);
    await rename(temporary, target);
  } finally {
    await rm(temporary, { force: true });
  }
}
