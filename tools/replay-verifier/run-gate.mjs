// th13 replay-verifier：门禁驱动——候选清单 vs oracle 清单逐帧逐字段比对。
// 裁决 = 外层仓库 oracle/draw_compare.py 的 RESULT first_divergence_frame
// （唯一裁决，AGENTS.md 方法论）。golden 记录「当前最佳首分歧帧」为退化
// 门（新跑不得差于 golden 基线；改进须回写 golden——基线只升不降）。
//
// 用法（容器内 /work/eagler-touhou/ports/th13）：
//   node tools/replay-verifier/run-gate.mjs [--case <id>] [--lane quick|daily|all]
//     [--candidate-root <dir>] [--golden <file>]
// 契约：--ignore 类过滤在本门禁内【禁止使用】（整改纪律：绿灯链路不得
// 遮蔽字段；tfactor 残留态按 HANDOFF 工作面销账）。
import {spawnSync} from 'node:child_process';
import {readFileSync, writeFileSync, existsSync, mkdirSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

const here = fileURLToPath(new URL('./', import.meta.url));
const corpus = JSON.parse(readFileSync(resolve(here, 'corpus.json')));
const option = name => {
  const i = process.argv.indexOf(name);
  return i < 0 ? null : process.argv[i + 1];
};
const wanted = option('--case');
const lane = option('--lane') || 'all';
const ids = wanted ? [wanted]
    : lane === 'all' ? Object.values(corpus.suites).flat()
                     : corpus.suites[lane];
if (!ids) throw Error('Unknown suite: ' + lane);
const candidateRoot = option('--candidate-root') ||
    resolve(here, '../../artifacts/replay-verifier');
const goldenPath = option('--golden') ||
    resolve(here, 'golden', 'baseline.json');
const accept = process.argv.includes('--accept-golden');

const oracleRoot = '/work/artifacts/oracle-draws';  // 外层仓库容器映射
const compare = (candidate, oracle) => {
  const run = spawnSync('python3', ['-m', 'oracle.draw_compare', candidate, oracle],
                        {cwd: '/work', encoding: 'utf8'});
  if (run.status === null) throw Error('draw_compare failed to start: ' + run.error);
  const out = run.stdout.trim();
  const m = out.match(/RESULT first_divergence_frame=(\S+) window=(\d+) ops=(\d+)/);
  if (!m) throw Error('No RESULT line from draw_compare (exit ' + run.status + '):\n' +
                      out + '\n' + run.stderr);
  return {first: m[1], window: Number(m[2]), ops: Number(m[3]), exit: run.status,
          line: out.split('\n').pop()};
};

const golden = existsSync(goldenPath)
    ? JSON.parse(readFileSync(goldenPath, 'utf8'))
    : {schema: 'th13/replay-draws-golden/v1', cases: {}};
const results = [];
for (const id of ids) {
  const fixture = corpus.cases.find(c => c.id === id);
  const candidate = resolve(candidateRoot, id + '.candidate.jsonl');
  const oracle = resolve(oracleRoot, fixture.oracleManifest);
  if (!existsSync(candidate)) throw Error('Candidate missing (run run-oracle.mjs): ' + candidate);
  const verdict = compare(candidate, oracle);
  const prior = golden.cases[id];
  let status = 'PASS';
  if (verdict.first === 'none') status = 'PASS';
  else if (prior && prior.first_divergence_frame !== 'none') {
    // 只升不降：新首分歧帧 ≥ 基线才 BASELINE-HELD（分歧更晚=更好；旧代码
    // 方向写反——把改进判 REGRESSION、退化判 HELD，2026-10-23 f511 改进
    // 被 FAIL 时暴露）。
    const better = Number(verdict.first) >= Number(prior.first_divergence_frame);
    status = better ? 'BASELINE-HELD' : 'REGRESSION';
  } else if (prior && prior.first_divergence_frame === 'none') {
    status = 'REGRESSION';  // 曾全绿后退化
  } else {
    status = 'UNBASELINED';  // 无 golden 基线：首次登记，不算门禁失败
  }
  results.push({id, first_divergence_frame: verdict.first, window: verdict.window,
                ops: verdict.ops, status, line: verdict.line});
  if (accept) golden.cases[id] = {first_divergence_frame: verdict.first,
                                  window: verdict.window, ops: verdict.ops};
}
const failing = results.filter(r => r.status === 'REGRESSION');
const report = {schema: 'th13/replay-draws-gate/v1', lane,
                status: failing.length ? 'FAIL' : 'PASS', results};
console.log(JSON.stringify(report, null, 2));
if (accept) {
  mkdirSync(dirname(goldenPath), {recursive: true});
  writeFileSync(goldenPath, JSON.stringify(golden, null, 2) + '\n');
  console.log('golden updated: ' + goldenPath);
}
process.exitCode = failing.length ? 1 : 0;
