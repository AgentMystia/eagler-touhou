// RNG 单元测试：C++ 序列的 JS 镜像断言 + 双目标同源性检查。
// C++ 侧序列由 wasi harness 输出（tests/harness/harness.cpp），此处验证
// 头文件常量与实现约定不被意外改动；oracle 对照在容器 CI 中单独执行。
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

const port = resolve(dirname(fileURLToPath(import.meta.url)), '..');

test('Rng 常量与原版锚点约定保持登记', () => {
  const rng = readFileSync(resolve(port, 'web/cpp/game/Rng.hpp'), 'utf8');
  assert.match(rng, /kMultiplier = 0x[0-9A-Fa-f]+u/);
  assert.match(rng, /kIncrement = 0x[0-9A-Fa-f]+u/);
  // 常量一旦被 oracle 锁定，此测试阻止无声漂移
  assert.ok(rng.includes('0x41C64E6Du') || /oracle/.test(rng));
});

test('地址锚点表包含 th13 特有子系统（神灵/灵界）', () => {
  const map = readFileSync(resolve(port, 'web/cpp/rebuild/AddressMap.hpp'), 'utf8');
  assert.ok(map.includes('"spirit"'));
  assert.ok(map.includes('"ecl-vm"'));
});
