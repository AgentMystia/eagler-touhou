# 场景覆盖矩阵（coverage matrix）

> 覆盖矩阵唯一 home（AGENTS.md「覆盖矩阵与交付门禁」§1）。每阶段交付前
> 更新；**「未覆盖/销账条件」必须被下一阶段消费**，不得默认通过。
> 形态：行 = 场景窗（用户会看到的每一类画面），列 = 两侧链路状态。
> 状态取值：✅ 两侧对齐（附裁决数字）/ 🟡 窗已建待推进（附首分歧）/
> 📦 未建 / ⛔ 有意偏差（附理由）。
> 更新纪律：引用任何数字前重跑实测（比对工具纪律 §4——绿基线会腐烂）。

## 一、场景窗矩阵（裁决 = draw_compare RESULT / 事件窗 RESULT）

| 场景窗 | oracle 捕获面 | 移植侧导出 | 状态（2026-10-23 实测） | 备注 / 销账条件 |
|---|---|---|---|---|
| 标题窗 A（主菜单 600 帧） | `title_oracle.py` → `title-draws.jsonl` | harness `title` | 🟡 f487（81%） | f487 起 ins_408 首帧语义差——已按机制层优先原则冻结参数微调，升级引擎语义工作面（oracle VM 层写 watch）；触发判据登记 |
| 标题窗 B（开局链 750 帧） | `title_oracle.py` 窗 B | harness `title-b` | 🟡 f379 | 与窗 A 同根因族（f379=count 22≠20 属 A/B 输入序列差异面） |
| 对局窗·无输入（st01 3600 帧） | `render_oracle.py` → `st01-draws.jsonl` | harness `draws` | 🟡 f0（tfactor）/f61（敌机行走位差） | golden 已锚（replay-verifier：BASELINE-HELD）；事件流同窗 10187 **none** 硬基线 |
| 对局窗·回放（udSr01 11065 帧） | `st01-draws-replay.jsonl` | harness `draws-replay` | 🟡 f0（tfactor）/f61 | golden 已锚；**本窗是对局主线**：f61 敌机行走精灵 x 差 96px（ECL 运动链）、f0 tfactor 一帧滞后（装载趟插值残留态——`--ignore tfactor` 唯一登记项，销账=建模完成） |
| 难度窗 | ❌ 未建（真机行为已锚：蓝图 §8.6） | ❌ | 📦 | 入场条件：窗 A 的 ins_408 工作面销账后（难度屏构建期 vm120 自 spawn 崩溃已修） |
| 角色窗 | ❌ 未建（蓝图 §8.4/窗B 解剖部分完成） | ❌ | 📦 | 窗 B 前沿推进后 |
| HUD 窗 | ❌ 未建（捕获面研究：`hud-capture-plan.md`） | ❌ | 📦 | oracle 捕获面扩展：0x40e4d0 族不在现捕获面——需假设备扩展 |
| 暂停窗 | ❌ 未建（脚本 132-133 已锚） | ❌ | 📦 | 同 HUD 窗 |
| 结算/过关窗 | ❌ 未建（S3 研究入库：`results-flow.md`/`results-overlay-anatomy.md`） | ❌ | 📦 | S3 战役入场即建 |
| Boss 战窗 | ❌ 未建 | ❌ | 📦 | 对局窗主线收敛后 |
| Stage2+ | ❌ 未建 | ❌ | 📦 | 资产已备齐（st02 全套）；全链路验收口径含 Stage2 入场 |

## 二、捕获面边界（oracle 侧，§4 已知边界对位）

- 现覆盖：ANM mgr 两池全部精灵（敌机/ECL 挂接/装载预置）。
- **未捕获**：HUD、玩家、弹幕遍历（0x40e4d0 族）、关卡 ctx 内嵌 5 层——
  属 HUD/暂停窗的 oracle 前置（假设备捕获面扩展，S2 入场条件）。
- 纹理内容（THTX/GDI/后台拷贝）不比——契约既定（draw-contract §4.1）。

## 三、事件流窗（行为侧裁决）

| 窗 | 状态 | 数字（2026-10-23 实测） |
|---|---|---|
| st01 无输入 | ✅ | 3600 帧 / 10187 事件 `none`（本轮整改后复跑） |
| udSr01 回放（st01 段） | 🟡 | f122（**基线勘误**：HANDOFF 旧登记 2652 已漂移，本轮实测基线 commit 与整改后同=f122；oracle 侧基线文件需重锚——见 HANDOFF 已知缺口） |
| th13_01 回放（st01 段） | 🟡 | f2625（旧登记；**引用前须复跑**） |

## 四、有意偏差登记（⛔）

- 菜单项禁用语义（Replay/PlayerData/MusicRoom 分期禁用后灰态）——与真机
  10 项全亮不同，`title-anm-blueprint.md` §8.5 登记；分期功能补齐后销账。
- th13 分叉的设备态语义（TFACTOR 初始 0 等）非偏差：已参数化到本作配置点
  （渲染基座纪律 §1），蓝图 §8.8 登记。

## 五、门禁集（战报前全量复跑；ADAPTATION.md 为验收清单 home）

1. 容器单测四套：`bash tests/{anm,ecl,ui,replay}/run.sh`（62 例）。
2. `node --test tests/*.test.mjs`。
3. 浏览器渲染统计契约：`make shot CMD='node --test …/graphics-stats.test.mjs'`。
4. golden 对跑：`node tools/replay-verifier/run-oracle.mjs && run-gate.mjs`
   （退化即 FAIL——golden 只升不降，改进须 `--accept-golden` 回写）。
5. 双目标构建：`node portable/build.mjs` + `build-harness.mjs`。
