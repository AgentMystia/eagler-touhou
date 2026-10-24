# 场景覆盖矩阵（coverage matrix）

> 覆盖矩阵唯一 home（AGENTS.md「覆盖矩阵与交付门禁」§1）。每阶段交付前
> 更新；**「未覆盖/销账条件」必须被下一阶段消费**，不得默认通过。
> 形态：行 = 场景窗（用户会看到的每一类画面），列 = 两侧链路状态。
> 状态取值：✅ 两侧对齐（附裁决数字）/ 🟡 窗已建待推进（附首分歧）/
> 📦 未建 / ⛔ 有意偏差（附理由）。
> 更新纪律：引用任何数字前重跑实测（比对工具纪律 §4——绿基线会腐烂）。

## 一、场景窗矩阵（裁决 = draw_compare RESULT / 事件窗 RESULT）

| 场景窗 | oracle 捕获面 | 移植侧导出 | 状态（2026-10-24 实测） | 备注 / 销账条件 |
|---|---|---|---|---|
| 标题窗 A（主菜单 600 帧） | `title_oracle.py` → `title-draws.jsonl` | harness `title` | 🟡 f487（81%） | f487 起 ins_408 首帧语义差——已按机制层优先原则冻结参数微调，升级引擎语义工作面（oracle VM 层写 watch）；触发判据登记 |
| 标题窗 B（开局链 750 帧） | `title_oracle.py` 窗 B | harness `title-b` | 🟡 f379 | 与窗 A 同根因族（f379=count 22≠20 属 A/B 输入序列差异面） |
| 对局窗·无输入（st01 3600 帧） | `render_oracle.py` → `st01-draws.jsonl` | harness `draws` | 🟡 **f511 RAW**（2026-10-23 机制层双修后） | f61 位差（0x40000 屏幕原点 320）与 f0 tfactor（创建点色步进）已销账；新前沿 f511 = op[64] 顶点 y 1px（x87/SSE 数值语义面）；事件流同窗 10187 **none** 硬基线 |
| 对局窗·回放（udSr01 11065 帧） | `st01-draws-replay.jsonl` | harness `draws-replay` | 🟡 **f124 RAW**（同上） | **本窗是对局主线**：新前沿 f124 = op[16] count 6≠4（批内多一精灵——回放输入驱动的下一机制面） |
| 对局窗·真机战斗（th13_01 11675 帧） | **battle_oracle（M1 全绿）**：f0-1202 全窗零崩、HUD/敌弹/神灵/符卡/立绘全族入面（ops min=9 max=438）、真输入路径 u16→VK、死亡时间线与原局逐帧同拍（f278/f814/f1160+GameOver f1202） | ❌ | 🟡 oracle 面就绪（1210 战斗帧稳定） | **W-S1 参考基准窗（M2 消费）**；Continue 续关后段（f1213+）被结算排行渲染崩链卡住——分期 M4 |
| 难度窗 | ❌ 未建（真机行为已锚：蓝图 §8.6） | ❌ | 📦 | 入场条件：窗 A 的 ins_408 工作面销账后（难度屏构建期 vm120 自 spawn 崩溃已修） |
| 角色窗 | ❌ 未建（蓝图 §8.4/窗B 解剖部分完成） | ❌ | 📦 | 窗 B 前沿推进后 |
| HUD 窗 | ❌ 未建（捕获面研究：`hud-capture-plan.md`） | ❌ | 📦 | oracle 捕获面扩展：0x40e4d0 族不在现捕获面——需假设备扩展 |
| 暂停窗 | ❌ 未建（脚本 132-133 已锚） | ❌ | 📦 | 同 HUD 窗 |
| 结算/GameOver 窗 | 🟡 battle_oracle 已达 GameOver 入口（f1202 首触 0x43f570；f1213 排行渲染崩链定位） | ❌ | 📦 | **M4 W-RESULTS 第一任务**：0x43de80→0x47b16d→0x47aee5 CRT 块头校验恒 EINVAL（假 CRT 堆）；修复两路已备（真 CRT 堆头/wine 自举真 scoreth13.dat）；**结算排行文字族未捕获**登记（字体战役阶段2 以几何/纹理身份门禁） |
| Boss 战窗 | ❌ 未建 | ❌ | 📦 | 对局窗主线收敛后 |
| Stage2+ | ❌ 未建 | ❌ | 📦 | 资产已备齐（st02 全套）；全链路验收口径含 Stage2 入场 |

## 二、捕获面边界（oracle 侧，§4 已知边界对位）

- **battle_oracle（M1，对局窗真机面）**：ANM mgr 两池全部精灵 + 敌弹
  （bullet1/2/3.png 逐批）+ 神霊/符卡/立绘/立绘框 + HUD（front00/agauge/
  lifebar/cardbg/ename 全族）+ 玩家 pl00 + 效果族（etbreak/eff_base/
  eff_line/eff_aura）——HUD 窗原「未捕获」已在 battle_oracle 面销账。
- 旧 render_oracle（无输入窗）现覆盖：ANM mgr 两池全部精灵（敌机/ECL
  挂接/装载预置）。
- **未捕获**：①STD 舞台（stg1bg——两窗均零 op，0x404140 walk 不在捕获面，
  M2.0）；②结算排行文字族（0x43de80 族，M4——崩链+字体战役阶段2 以
  几何/纹理身份门禁）；③对话文字面（M2.5 关末链一并）。
- 纹理内容（THTX/GDI/后台拷贝）不比——契约既定（draw-contract §4.1）。

## 三、事件流窗（行为侧裁决）

| 窗 | 状态 | 数字（2026-10-24 实测） |
|---|---|---|
| st01 无输入 | ✅ | 3600 帧 / 10187 事件 `none`（2026-10-23 整改后复跑） |
| udSr01 回放（st01 段） | 🟡 | f122（2026-10-23 fresh oracle 基线复跑同值=真分歧；根因=卫星追尾弹模型两侧均为占位——**旧 bypass 基线的击杀/弹幕面从未对照原版（M1.3a 解剖实证）**；W-S1 以 battle_oracle 为准重建） |
| th13_01 回放（st01 段） | 🟡 | 旧登记 f2625 **作废**——旧基线同上不可作基准；battle_oracle 死亡时间线 f278/f814/f1160+GameOver f1202 与原局逐帧同拍（rpy 输入流+视频三重验证），事件 v3 状态探针（TH13_STATE_LOG）已落 |
| th13_01 战斗窗·状态流（v3） | 🟡 | 400 帧探针验证（score f41=15 起涨/keys 逐帧正确）；全窗基线待 M2 对拍前生成 |

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
