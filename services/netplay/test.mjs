import { readdir } from 'node:fs/promises';
import { spawn } from 'node:child_process';
const tests = (await readdir(new URL('./tests/', import.meta.url)))
  .filter((name) => name.startsWith('test-') && name.endsWith('.mjs'))
  .sort();
const child = spawn(
  process.execPath,
  [
    '--test',
    '--test-reporter=tap',
    '--test-concurrency=3',
    ...tests.map((name) => 'tests/' + name),
  ],
  { cwd: import.meta.dirname, stdio: 'inherit', shell: false },
);
child.once('error', (error) => {
  console.error(error.message);
  process.exitCode = 1;
});
child.once('exit', (code) => {
  process.exitCode = code ?? 1;
});
