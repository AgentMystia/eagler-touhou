# th16 路线图

交付标准：对任意稳定输入，移植版 = 原版（行为、画面、系统）；全部关卡、难度、机体 × 子季节；代码可读、性能达标。达标前不开 PR。

## 已完成

| 里程碑 | 证据 |
| --- | --- |
| 环境：emsdk 6.0.9 / wasi-sdk 34 / Ghidra 12.1.3 / thtk / python venv | `docs/DEVELOPMENT.md` |
| Ghidra 全函数导出（1761 函数） | `artifacts/th16/decomp/jp/` |
| RNG 16/32 位 + 浮点变体位级一致（7 种子 × 64 步 + 16 浮点步） | `tests/rng.test.mjs`、`tests/golden/rng.json` |
| THA1 档案 / LZSS / 加密：215 条目逐字节一致 | `tests/archive.test.mjs` |
| ANM v8 解析（entry/sprite/script/哨兵）+ THTX 四格式逐像素一致 | `tests/archive.test.mjs`、`tools/oracle/thtx-compare.py` |
| SDL3 + GLES 3.0 运行时：th16.dat 整档进 MEMFS，启动幕 `sig.anm` 静态帧上屏 | `web/cpp/sdl/`、`web/sdl-runtime/th16.html` |
| ANM v8 VM 与管理器（0x45f980 / 0x46a3a0 族）：0x5fc 结构体逐字段镜像、全部指令、31 种缓动、计时器、VM 池与链表、子 VM；54 个 ANM 文件 1887 个脚本 × 60 帧 VM 状态与 layer 绘制链对 Unicorn 直跑原版逐字节一致；wasm32 与本机输出相同；3600 帧 < 0.3s | `game/AnmVm.cpp`、`game/AnmManager.cpp`、`tests/anm-vm.test.mjs`、`tools/oracle/anm-oracle.py` |
| ANM 屏幕四边形绘制（0x468490 mode 0..3）+ 启动幕改由 VM 驱动：`sig.anm` script0/1 淡入上屏；纹理按 entry 声明尺寸衬垫；几何单测 + 浏览器截图 vs 纹理合成 MAD≈1.8 | `game/AnmRenderer.cpp`、`sdl/Application.cpp`、`tests/anm-draw.test.mjs` |
| Supervisor 场景状态机 + 输入边沿：Boot→Title（0x43b290 请求；标题线程等 0xb4 帧，仅 DAT_004c1800&0x180 可放行）；`title.anm`/`title_v.anm` 装载 | `game/Supervisor.hpp`、`game/Input.hpp`、`tests/supervisor.test.mjs` |
| Chain 表（0x401730/0x401300/0x4013b0/0x401460/0x4015a0/0x4017a0：优先级插入、init、启用位、Remove/Break/Restart 返回码）+ 帧计时 0x45a8a0（1/60，rAF 折算 0..4 步）+ 0x405580 边沿/连发（>0x19 帧后每 8 帧）| `game/Chain.cpp`、`game/FrameClock.hpp`、`game/Input.hpp`、`tests/supervisor.test.mjs` |
| 标题子画面 0x44fe20 难度 / 0x4502c0 机体 / 0x450af0 子季节 / 0x450ef0 练习关卡：面板脚本 0x7e/0x96/0x97/0x71、项中断（cursor+7/+0xd/+0x13/+0x19）、确定/取消节拍（Timer 0xd/0xe/5/10/0x27）、MenuCursor 压栈/出栈、GameGlobals（难度 1 / 机体 / 季节 / 关卡 / 模式位）；季节确定 → DAT_004c17c4=7 → Supervisor 切到 Game(7) 并析构标题（0x44af50：摘链、kill 槽 0x10/0x11）；Quit → 场景 2；CDP 截图 RANK/PLAYER/SUBWEAPON SELECT | `game/TitleScene.cpp`、`game/GameGlobals.hpp`、`game/MenuCursor.hpp`、`game/GameSession.cpp`、`tests/supervisor.test.mjs` |
| ANM mode 8（0x467410）：世界矩阵（0x3d0 精灵尺寸/256 × scale × 欧拉顺序 ins_437 × 0x406a70 平移）经 UI 相机（0x43cb10/0x43c780：fov π/6、eye z=(h/2)/tan(fov/2)、up=(0,-1,0)、PerspectiveFovLH 1..10000）投影，±128 单位四边形 × 9 锚点（0x46b900）、纹理矩阵 0x450、TFACTOR 颜色、Z 写（0x43c530）；缺省设备状态 0x45ba80（ZFUNC ALWAYS、alpha test ref 1、寻址 WRAP）；ins_300(-1) 的 ascii 0x102 精灵来源槽；落叶发射器为标题 VM 表 +0x73c（0x11f），析构 0x44ad20 对其 interrupt 1；季节面板翻转 / 子季节项 CDP 截图可见；WASI 与本机一致 | `game/AnmRenderer.cpp`、`game/AnmVm.hpp`、`game/TitleScene.hpp`、`game/GameSession.cpp`、`sdl/Application.cpp`、`tests/supervisor.test.mjs` |
| 标题主菜单 0x44af80/0x44b5f0 case 0/1/2/4：装饰 0x61/0x65 → Timer 0x78 → logo/title_v → 第 11 帧 Main；MenuCursor（wrap、禁用 Extra）；确定 0x80001 / 取消 0x102；子项 0x1d/0x1e/0x1f 中断（0x46f510 子树查找）；Timer>0x13 跳转画面表；逻辑层抽成 `GameSession`（浏览器与 harness 同一份时序）；CDP 截图光标移动/取消跳 Quit/确定 | `game/TitleScene.cpp`、`game/MenuCursor.hpp`、`game/GameSession.cpp`、`game/AnmManager.cpp`、`tests/supervisor.test.mjs` |
| Game 场景 7 骨架：GameManager（0x42d700/0x42cb60/0x42d7b0/0x42dc50/0x42d200：更新链 0xf、标志位、计时、DAT_004a579c/57a0）+ 关卡表 DAT_004a22d0（8 × 0xd4）+ STD 装载 0x40ac30（头/对象/四边形/实例/脚本，7 关全部解析，全部四边形 type 0）+ StageManager（0x4097c0/0x409db0/0x40add0/0x409e50/0x40b3b0/0x40aed0/0x40b2f0：相机 Interp、脚本 0..0x14（0xf/0x10 空）、摇晃 7 种、雾 Interp、层精灵 8 个、面 VM 共享 + 逐实例 entity_offset / 尺寸缩放）+ 背景绘制 0x409f90/0x40af70/0x40c280（相机矩阵 0x43c940 LookAtLH/PerspectiveFovLH zn 30 zf 8000、视口 (116,4,408,472)、顶点线性雾 1/rhw、ZENABLE=!(lo&0x2000)、层精灵无雾）；GLES 顶点雾路径（specular.a）与矩形清色；st01 脚本时间线（mode 4 缓动 / 线性 / 雾 720 帧）与 8 块地砖雾因子单测；CDP 截图第 1 关森林 + 天空层精灵；WASI 3900 帧 < 0.5s | `game/GameManager.cpp`、`game/Stage.cpp`、`game/Std.cpp`、`game/StageTable.hpp`、`game/AnmRenderer.cpp`、`sdl/GraphicsDevice.cpp`、`tests/stage.test.mjs` |
| StageManager Unicorn oracle（`tools/oracle/stage-oracle.py`：原版 0x409db0 → 0x40add0 → 0x409e50 直跑，桩 0x46d020 / D3DXVec3Normalize）：7 关 × 6000 帧的相机 / 脚本计时 / 雾 / 面 VM / 8 层精灵 VM 原始字节逐帧一致（golden 摘要 `tests/golden/stage.json`，harness `stage-run` / `stage-digest`，wasm32 同摘要）。由此修正：层精灵与面 VM 的原版全零初态（file_slot 0 → 每帧推进 text.anm 脚本 0）、畸变半径² 0x4b12a310 = 3100²、摇晃 sin/cos 须经 double 再舍入、ANM 中断标签缺失时不再置 kStopped（0x4629fa） | `tools/oracle/stage-oracle.py`、`tests/golden/stage.json`、`game/Stage.cpp`、`game/AnmVm.cpp` |

## 顺序

1. Game 场景（DAT_004c17c4=7）余项：RT 合成链（绘制链 1/0xe/0xf/0x38/0x39：DAT_004c127c/1280/1284、front.anm RT 精灵 0x4c128c/0x4c1298——当前以 GL 视口裁剪 + x−96 代替）、绘制链 6（ANM 层 0x20/0x21 用 3D 相机）、视锥剔除 0x40a7d0、畸变 0x40c4a0、ins 17 效果；Gui 0x447e20 / Player 0x427ca0 / Bullet 0x411dd0 / Enemy+ECL 0x41b340 / Item 0x4314a0 / Bomb 0x40d890 / 0x42f440 / 0x43e5a0 / 0x449e50 / 0x4178a0 管理器；BGM 0x43c370；季节确定后过场（EffectManager 0x418ba0 + ascii 0x11）；关卡结束/切换（flags 0x70/0x4000/0x10000）。
2. 标题余项：Option 0x44c570、Replay 0x451750、Player Data 0x452330、Music Room 0x4546f0、Manual 0x4545a0、Spell Practice 0x4553d0；demo 录像触发（无输入 0x708 帧）；标题绘制链 0x44b530；存档（scoreth16.dat）驱动的 Extra/练习解锁。
3. ANM 其余绘制模式（4..7 / 9+ / 0x10.. 矩形线段：投影/条带/网格/特效）、mode 0 的渐变色位（0x60000）、全局色乘 0x1c7fd88 与外部钩子：ins_508 EffectManager（0x418af0）、sprite_mapper（0x491b1c）、update/draw/delete 回调表（0x4919e8 / 0x491b58）；标题画面逐帧对照。
4. ECL VM（0x41dcb0 族）：指令集、线程/子程序、变量、与 ANM 绑定。
5. 敌机 / 子弹 / 激光 / 道具管理器；碰撞；分数/擦弹。
6. 玩家与 4 机体 × 4 子季节（pl00–pl03 + .sht）；季节系统（Season gauge、Release）。
7. Boss、符卡、符卡练习；STD 背景场景。
8. 标题 / 菜单 / Option / Key Config / Music Room / Result / Player Data；MSG 对话；Ending / Staff。
9. Replay 录制与回放（`th16_udXXXX.rpy`）、score / cfg 文件互通。
10. 音频：thbgm.fmt 循环点、音效表。
11. 全关卡 oracle 对拍：原版录像在移植版回放逐帧状态一致；关键帧像素对照。
12. 性能：60fps、单帧 < 8ms、WASI 3600 帧 < 10s。
13. 登记（workspace.json、runtime-builds.json、product-catalog、docs/migration、TOUHOU_PROGRESS、ports/README、WORKSPACE）、ADAPTATION.md 验收记录、PR。

每一步：先 oracle，再实现，再 `node --test`；AddressMap.hpp 同步登记。
