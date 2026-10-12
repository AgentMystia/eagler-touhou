import { readdir } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import '../scripts/ui/check-runtime.mjs';

const root = fileURLToPath(new URL('../', import.meta.url));
const artifactTests = new Set([
  'app-shell-build.test.mjs',
  'development-server.test.mjs',
  'external-frontend-artifact.test.mjs',
  'initial-presentation.test.mjs',
  'refresh-react-artifact.test.mjs',
  'server-react-artifact.test.mjs',
]);
const args = process.argv.slice(2);
if (args.length > 1 || (args.length && !/^--group=(unit|artifact|all)$/.test(args[0])))
  throw new Error('用法：node tests/run-ui.mjs [--group=unit|artifact|all]');
const group = args[0]?.slice('--group='.length) ?? 'all';
const jobs = Number(process.env.EAGLER_UI_TEST_JOBS ?? 3);
if (!Number.isSafeInteger(jobs) || jobs < 1) throw new Error('EAGLER_UI_TEST_JOBS 必须是正整数');
const names = (await readdir(resolve(root, 'tests/ui')))
  .filter((name) => name.endsWith('.test.mjs'))
  .sort();
const groups = group === 'all' ? ['unit', 'artifact'] : [group];
let failed = false;
for (const selected of groups) {
  const tests = names.filter((name) => artifactTests.has(name) === (selected === 'artifact'));
  if (!tests.length) throw new Error('没有找到测试组：' + selected);
  console.log('UI ' + selected + ': ' + tests.length + ' files');
  // 真实产物构建会使用同一输出目录和预览端口，因此串行执行。
  const code = await new Promise((done, reject) => {
    const child = spawn(
      process.execPath,
      [
        '--test',
        '--test-reporter=tap',
        '--test-concurrency=' + (selected === 'artifact' ? 1 : jobs),
        ...tests.map((name) => 'tests/ui/' + name),
      ],
      { cwd: root, stdio: 'inherit', shell: false },
    );
    child.once('error', reject);
    child.once('exit', (code) => done(code ?? 1));
  });
  failed ||= code !== 0;
}
if (failed) process.exitCode = 1;
