import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, mkdir, readFile, writeFile, rm, readdir } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import { resolve } from 'node:path';
import { tmpdir } from 'node:os';
import { archiveDirectory } from '../lib/archive-directory.mjs';

test('ZIP round-trips binary files, Unicode names and empty input directories', async () => {
  const work = await mkdtemp(resolve(tmpdir(), 'archive-directory-'));
  const source = resolve(work, 'input'), archive = resolve(work, 'output.zip');
  try {
    await mkdir(resolve(source, 'games/th15'), { recursive: true });
    await writeFile(resolve(source, '存档.bin'), Buffer.from([0, 128, 255, 10]));
    await archiveDirectory(source, archive, { prefix: 'payload' });
    const python = process.env.PYTHON ?? (process.platform === 'win32' ? 'python' : 'python3');
    const result = JSON.parse(execFileSync(python, ['-c',
      'import zipfile,json,sys; z=zipfile.ZipFile(sys.argv[1]); print(json.dumps({n:list(z.read(n)) for n in z.namelist()}))', archive], { encoding: 'utf8' }));
    assert.deepEqual(result, { 'payload/games/th15/': [], 'payload/存档.bin': [0, 128, 255, 10] });
    const original = await readFile(archive);
    await assert.rejects(archiveDirectory(source, archive));
    assert.deepEqual(await readFile(archive), original);
    await assert.rejects(archiveDirectory(source, resolve(work, 'bad.zip'), { prefix: '../outside' }));
    assert.deepEqual((await readdir(work)).sort(), ['input', 'output.zip']);
  } finally { await rm(work, { recursive: true, force: true }); }
});
