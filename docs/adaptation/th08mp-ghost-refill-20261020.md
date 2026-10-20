# th08-mp 幽冥组进面补弹修复验证记录 | 2026-10-20

分支：`fix-th08mp-ghost-team-stage-bomb`（基于上游 main `ed196505`）。
范围：仅 `ports/th08-mp`（多人变体），普通版 `ports/th08` 不受影响（改动文件全部在 `#error` 守卫的 multiplayer 头/ifdef 块内，构建隔离测试确认普通构建仍排除 multiplayer 实现）。

## 检查编号

覆盖 `ports/th08-mp/ADAPTATION.md` 的 MP-RULE-02（个人资源与团队资源，跨面资源归属与数值）。

## 根因

原版机制在 `ports/th08/web/cpp/game/Dialogue.cpp:78`（`Dialogue::stage_results()`）：非最终面、角色 3/10/11（幽冥组、单妖梦、单幽幽子）、炸弹 <3 时 `add_bombs(1)`。th08-mp 原样保留该行但完全失效：

1. `Dialogue` 绑定的 `values` 是共享原生 `GameValues`（seat-0 旁车），而 MP 每席炸弹的真实存储是 `session.pilot_resources[seat]`（HUD 与全部游戏逻辑读它，`WorldJournal.cpp:37` 已入回滚日志）——补弹写入无人消费的旁车。
2. `dialogue_context.character` 是主机角色（`GameplayScene.cpp:232`），客机选幽冥组时门控条件不成立。

## 修复（6 文件，+66/−5）

| 文件 | 改动 |
| --- | --- |
| `web/cpp/multiplayer/CooperativeResources.hpp` | `begin_next_stage_life` 增加 `ghost_refill` 参数（补弹 <3 时 +1，与原版 `Scalar::truncate` 判定同源）；新增 `netherworld_stage_bonus(u8)` 谓词（3/10/11） |
| `web/cpp/game/GameplayScene.cpp` | 每席进面循环按 `retain_counters && netherworld_stage_bonus(player_characters[seat])` 传入——仅过关进面路径触发（首局/暂停重开/Extra 均被 `retain_counters` 排除），每席用自己的角色判定 |
| `web/cpp/multiplayer/SessionSetup.cpp` | `gameplay_contract` 规则字 `0x08000015`→`0x08000016`（仓库历次规则修订惯例），旧录制在载入时被干净拒绝而非跨面 desync |
| `tests/support/previous-rule-contract.hpp` | 冻结 `previous_ghost_contract()`（0x08000015 旧合同） |
| `tests/netplay-runtime-test.cpp` | 断言旧 ghost 合同 ABI 被 `ContractMismatch` 拒绝 |
| `tests/player-life-rules-test.cpp` | `netherworld_stage_refill()`：12 角色门控穷举、bombs {0,1,2}→+1、{3,4,8} 不变、灵体重置后判定、非幽冥组不补、默认参数行为不变 |

回滚一致性：修复只写 `session.pilot_resources`（已入回滚日志），两侧确定性逻辑同帧同值；netplay 契约测试确认混版本被合同拒绝。

## 验证（全部一次性 podman 容器，`--rm`，结束后 `podman ps -a`/`network ls` 确认零残留）

工具链：wasi-sdk 34.0 官方 deb（sha256 `09f37a4781…908d4a` 与 CI pinned 一致）、emscripten/emsdk 6.0.10（上游 DEVELOPMENT.md 标注 6.0.9，差异仅补丁版本）、node 23、python 3.12-bookworm + Playwright chromium。
构建身份：multiplayer 变体 wasm sha256 `5541f7ce9bcfe16e1201d8c0ab9f1a60fe0afa97bd53db7e801ea08e26f07476`。
common 来源：同仓库 `common/`（未改动）。
环境：Linux CachyOS x64，容器化 podman（node:23、emscripten/emsdk、python:3.12-bookworm），Chromium headless shell 156.0.8078.4。
网络：本地双席路由（`mp_fixture_local_route`）；真实 RTC 双实例在容器环境未通过（见下文限制说明）。

### A. 单元/契约（WASI，无需游戏资产）

- `node portable/check-multiplayer-rules.mjs` → **PASS**（SP+MP 双构建；MP 输出 "144 death cases plus final death and Netherworld refill"，含新断言）
- `node portable/check-multiplayer-netplay.mjs` → **PASS**（含 previous_ghost_contract 拒绝断言）
- `node portable/check-multiplayer-cooperation.mjs` → **PASS**（cooperative-lifecycle）
- `node portable/check-multiplayer-replay.mjs` → **PASS**（Replay 归档/原子拒绝）
- `node portable/check-multiplayer-resources.mjs` → **PASS**（player-resources）

### CI 镜像（ports/th08-mp 两个作业的本地复刻）

- source-contract：`controller-core.test.mjs`+`release-contract.test.mjs`（8/8）、`multiplayer-build-isolation.test.mjs`（4/4）、`check-high-refresh-contract.mjs`、`check-presentation-purity.mjs` → **全部 PASS**
- golden-integrity：`node tools/replay-verifier/verify-golden.mjs` → **PASS**（quick 35974 ticks；daily 152356 ticks）

### B. 全量编译

- `node portable/build.mjs --th08 --multiplayer` → **EXIT=0**，产出 `web/artifacts/multiplayer/th08-sdl.wasm`（3.7MB，wasm sha256 `5541f7ce…07476`）
- `node portable/build.mjs --th08 --multiplayer-fixtures` → **EXIT=0**（诊断变体）

### C. 浏览器真实游戏世界 E2E（Playwright chromium + fixture 构建 + 零售 th08.dat）

- 基线：上游 `portable/multiplayer/check-cooperation.py` 全 15 用例 → **PASS**（证明 fixture 服务、th08.dat、合成字体资产工作正常）
- **幽冥组跨面断言**（`check-ghost-refill-local.py`，本地双席真实游戏世界，`stageClearSetup()` 走与 check-stage-boundary.py 相同的原生结算时钟，真实跨面 Stage 0→1）：
  - 房间 [幽冥组(3), 结界组(0)]：进面后炸弹 **[2→3, 2→2]**——幽冥组 +1，同伴不变 ✓
  - 房间 [结界组(0), 咏唱组(1)]：进面后 **[2→2, 2→2]**——无人补弹 ✓
  - 结果 JSON：`/tmp/th08mp-verify/ghost-refill-local.json`（passed: true）
- 合成资产说明：`cp932.bin`（131072 字节恒等映射表，通过 `Cp932::load` 尺寸校验）与 `blend.bin`（131072 字节全 0，每字节 ≤31 满足 `load_blend` 校验）、msgothic 用 Noto Sans CJK 替代——仅字形渲染，不影响逻辑帧与资源断言。

### E2E 环境限制说明

双实例真实 RTC（`check-stage-boundary.py` 协议）在容器化环境（共享网络命名空间 + 单核限制下的双 iframe WASM 实例）于正常游玩帧出现 `Confirmed peer input stopped advancing`（frame ~40），**上游自带脚本在此环境同样卡帧**，与本次修复无关（修复只改跨面资源初始化，卡点在正常帧）；传输/回滚层正确性已由 A 组 netplay/cooperation WASI 套件与 cooperation 浏览器基线覆盖，故跨面行为断言采用本地双席路由完成。

## 残留清理

`podman ps -a`：无 th08mp 相关容器；`podman network ls`：无 th08mp-e2e 网络。临时目录 `/tmp/th08mp-verify/`（SDK、字体合成、E2E JSON 证据）与构建产物（gitignored）保留供审计复核，可用后即删。
