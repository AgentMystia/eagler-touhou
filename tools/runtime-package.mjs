import { createHash, randomUUID } from 'node:crypto';
import { lstat, mkdir, mkdtemp, readFile, readdir, rename, rm } from 'node:fs/promises';
import { dirname, isAbsolute, resolve } from 'node:path';

export async function verifyRuntimeDirectory(directory, game) {
  const root = resolve(directory);
  const inventory = JSON.parse(await readFile(resolve(root, 'runtime-files.json'), 'utf8'));
  if (inventory.schema !== 'eagler-touhou/runtime-directory/1' || !inventory.files || typeof inventory.files !== 'object' || Array.isArray(inventory.files))
    throw new Error('无效的 Runtime 文件清单');
  const expected = new Set(['runtime-files.json']);
  for (const [name, identity] of Object.entries(inventory.files)) {
    if (!name || isAbsolute(name) || /[\\:\0]/.test(name) || name.split('/').some(part => !part || part === '.' || part === '..'))
      throw new Error('无效的 Runtime 路径：' + name);
    if (expected.has(name)) throw new Error('重复的 Runtime 路径：' + name);
    expected.add(name);
    if (!Number.isSafeInteger(identity?.bytes) || identity.bytes < 0 || !/^[0-9a-f]{64}$/.test(identity.sha256))
      throw new Error('无效的 Runtime 文件身份：' + name);
  }
  const actual = new Set();
  async function visit(path, prefix = '') {
    for (const entry of await readdir(path, { withFileTypes: true })) {
      const name = prefix + entry.name;
      if (entry.isSymbolicLink()) throw new Error('Runtime 不接受符号链接：' + name);
      if (entry.isDirectory()) await visit(resolve(path, entry.name), name + '/');
      else if (entry.isFile()) actual.add(name);
      else throw new Error('Runtime 不接受特殊文件：' + name);
    }
  }
  await visit(root);
  if (actual.size !== expected.size || [...actual].some(name => !expected.has(name))) throw new Error('Runtime 文件集合与清单不一致');
  for (const [name, identity] of Object.entries(inventory.files)) {
    const bytes = await readFile(resolve(root, name));
    if (bytes.length !== identity.bytes || createHash('sha256').update(bytes).digest('hex') !== identity.sha256)
      throw new Error('Runtime 文件身份不符：' + name);
  }
  const manifest = JSON.parse(await readFile(resolve(root, 'manifest.json'), 'utf8'));
  if (manifest.game !== game) throw new Error('Runtime 游戏身份不符');
  return inventory;
}

/** 先生成并验证完整目录，再替换已验证的旧构建；失败时保留旧目录。 */
export async function publishRuntimeDirectory({ output, game, produce }) {
  const target = resolve(output);
  await mkdir(dirname(target), { recursive: true });
  let entry;
  try { entry = await lstat(target); }
  catch (error) { if (error.code !== 'ENOENT') throw error; }
  const previous = !!entry;
  if (entry) {
    if (!entry.isDirectory() || entry.isSymbolicLink()) throw new Error('输出必须是普通 Runtime 目录');
    await verifyRuntimeDirectory(target, game);
  }
  const stage = await mkdtemp(resolve(dirname(target), '.runtime-stage-'));
  const backup = target + '.previous-' + randomUUID();
  try {
    await produce(stage);
    await verifyRuntimeDirectory(stage, game);
    if (previous) await rename(target, backup);
    try { await rename(stage, target); }
    catch (error) {
      if (previous) {
        try { await rename(backup, target); }
        catch (restore) { throw new AggregateError([error, restore], '输出替换失败，旧 Runtime 保留在 ' + backup); }
      }
      throw error;
    }
    if (previous) await rm(backup, { recursive: true });
    return target;
  } finally { await rm(stage, { recursive: true, force: true }); }
}
