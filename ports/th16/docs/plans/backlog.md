# th16 待办

对照 `docs/DECOMP.md`、`web/cpp/rebuild/AddressMap.hpp` 与黄金测试后的未达交付项。`DECOMP.md` 已登记对齐的系统（Laser、Bomb、Gui、标题各画面逻辑、过关、回放文件、EffectManager、魔理沙激光、RT 合成、视锥剔除、关卡畸变）不在此列。`docs/plans/roadmap.md`「顺序」早于上述登记，不以它为准。

`flags & 0x1000` 矩形体碰撞（0x443af0 / 0x4049c0）无 ECL 设置路径，不排期。钩子 6（0x46a330，ANM 渲染模式 10）全部 `.anm` 无 302(10)，数据不可达，不排期。

## 开放项

1. 已关闭：难度画面绘制窗口第 344–353 帧已对拍。见「已关闭」。
2. 已关闭：Extra 四季 600 帧矩阵已锁。见「已关闭」。
3. 标题画面绘制对拍已关闭。见「已关闭」。
4. ANM mode 6（0x466820）：`artifacts/th16/dat` 全部 54 个 `.anm` 的 ins_302 渲染模式只有 0/1/2/3/4/8/21，没有 6。与钩子 6 相同，数据不可达，不排期。`AddressMap.hpp` 的空登记保留为说明。
5. 全关卡 × 难度 × 机体 × 子季节的长程像素对拍。600 帧摘要与若干绘制窗口已锁。第 1–6 面四难度四机体春、以及 Easy、Normal、Hard、Lunatic 四机体夏/秋/冬、Extra 四机体春夏秋冬第 590–599 帧顶点已对拍。第 1–6 面四难度四机体四季与 Extra 四机体春夏秋冬土用、空闲输入第 2000–2009 帧顶点已对拍。第 1–6 面四难度、四机体、四季在空闲与矩阵射击输入下的第 4000–4009 帧抽样已对拍。第 1–6 面 Easy 灵梦春空闲第 6000–6009 帧，以及第 3 面 Normal 灵梦春、第 6 面 Lunatic 灵梦冬空闲第 6000–6009 帧、Extra 灵梦春空闲第 4000–4009 帧已对拍。通关全程与其余长窗口未覆盖。
6. 性能：Snapdragon 865 级 60fps。本机无该设备，不能记为通过。WASI 3600 帧 <10s：本机没有 wasi-sdk，不能记为通过。
7. 登记：`launcher/config/workspace.json`、`runtime-builds.json`、`product-catalog`、`docs/migration`、`docs/TOUHOU_PROGRESS.md`、`ports/README`、`WORKSPACE.md`、`ADAPTATION.md` 验收。黄金交付标准达成前不开 PR。
8. 版本文件 0x43b480（`th16_%.4x%c.ver`）：开机 0x43b520 读取并写入 DAT_004c1af0 / DAT_004c1aec，退出路径 0x43b681 释放缓冲。没有其它读者。宿主落盘可后接，不阻塞对拍。

## 已关闭

- 难度画面绘制窗口第 344–353 帧已对拍（第 313 帧确认进入，第 334 帧 Difficulty，第 342 帧光标下移）。测试 `difficulty screen: 10 draw frames around a cursor-down match original structure and vertices`。
- Option 画面绘制窗口第 425–434 帧已对拍（第 376 帧确认进入，第 397 帧 Option）。未读 `th16.cfg` 时配置块保持 BSS 初值 0，`0x44dc70` 按该音量藏前导零。测试 `option screen: 10 settled draw frames match original structure and vertices`。
- Replay 画面绘制窗口第 360–369 帧已对拍（第 322 帧确认，第 343 帧 Replay，第 351 帧子状态 2）。测试 `replay screen: 10 settled draw frames match original structure and vertices`。
- Player Data 画面绘制窗口第 362–371 帧已对拍（第 324 帧确认，第 345 帧进入，第 353 帧子状态 2）。测试 `player data screen: 10 settled draw frames match original structure and vertices`。
- Music Room 画面绘制窗口第 370–379 帧已对拍（第 326 帧确认，第 347 帧进入，第 362 帧子状态 2）。测试 `music room: 10 settled draw frames match original structure and vertices`。
- Spell Practice 画面绘制窗口第 360–369 帧已对拍（第 320 帧确认，第 341 帧进入，第 353 帧子状态 2）。测试 `spell practice: 10 settled draw frames match original structure and vertices`。
- Manual 画面绘制窗口第 360–369 帧已对拍（第 330 帧确认，第 351 帧进入）。help.anm 由装载线程写入，标题路径不跑该线程，目录项不出现。测试 `manual screen: 10 settled draw frames match original structure and vertices`。
- Extra 四季 600 帧矩阵已锁：第 7 面 4 机体 × 春夏秋冬（子季节 0..3）× 难度 Extra，16 组十个摘要字段与 `game-oracle.py` 一致，写入 `tests/golden/stage-matrix-st7.json`，原有 4 条土用 case 保留，由 `tests/stage-matrix.test.mjs` 驱动 harness `game-digest`。
- 第 1–6 面 Easy 灵梦夏/秋/冬第 590–599 帧绘制顶点已对拍。测试 `stage 1–6 Easy Reimu summer/autumn/winter: frames 590–599 match original vertices`。
- Extra 四机体春夏秋冬第 590–599 帧绘制顶点已对拍。测试 `Extra four shots spring/summer/autumn/winter: frames 590–599 match original vertices`。
- 第 1–6 面 Normal 四机体夏/秋/冬第 590–599 帧绘制顶点已对拍。测试 `stage 1–6 Normal four shots summer/autumn/winter: frames 590–599 match original vertices`。
- 第 1–6 面 Hard 四机体夏/秋/冬第 590–599 帧绘制顶点已对拍。测试 `stage 1–6 Hard four shots summer/autumn/winter: frames 590–599 match original vertices`。
- 第 1–6 面 Lunatic 四机体夏/秋/冬第 590–599 帧绘制顶点已对拍。测试 `stage 1–6 Lunatic four shots summer/autumn/winter: frames 590–599 match original vertices`。
- 第 1–6 面四难度四机体春第 590–599 帧绘制顶点已对拍。测试 `stage 1–6 four shots spring four difficulties: frames 590–599 match original vertices`。
- 第 1–6 面四难度四机体四季与 Extra 四机体春夏秋冬土用、空闲输入第 2000–2009 帧绘制顶点已对拍。测试 `stage 1–6 and Extra idle: frames 2000–2009 match original vertices`。
- 第 1–6 面四难度、四机体、四季在空闲与矩阵射击输入下的第 4000–4009 帧抽样已对拍。测试 `stage 1–6 idle and firing: frames 4000–4009 match original vertices`。
- 第 1 面 Easy 灵梦春、第 3 面 Normal 灵梦春、第 6 面 Lunatic 灵梦冬空闲第 6000–6009 帧，以及 Extra 灵梦春空闲第 4000–4009 帧已对拍。测试 `stage 1, 3, 6 and Extra: frames 6000–6009 and Extra 4000–4009 match original vertices`。
- 第 1–6 面 Easy 灵梦春空闲第 6000–6009 帧已对拍。测试 `stage 1–6 Easy Reimu spring idle: frames 6000–6009 match original vertices`。
- 第 1–6 面 Easy 琪露诺/文/魔理沙夏/秋/冬第 590–599 帧绘制顶点已对拍。测试 `stage 1–6 Cirno/Aya/Marisa summer/autumn/winter: frames 590–599 match original vertices`。
