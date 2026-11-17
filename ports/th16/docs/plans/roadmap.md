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
| 标题主菜单 0x44af80/0x44b5f0 case 0/1/2/4：装饰 0x61/0x65 → Timer 0x78 → logo/title_v → 第 11 帧 Main；MenuCursor（wrap、禁用 Extra）；确定 0x80001 / 取消 0x102；子项 0x1d/0x1e/0x1f 中断（0x46f510 子树查找）；Timer>0x13 跳转画面表；逻辑层抽成 `GameSession`（浏览器与 harness 同一份时序）；CDP 截图光标移动/取消跳 Quit/确定 | `game/TitleMenu.hpp`、`game/MenuCursor.hpp`、`game/GameSession.cpp`、`game/AnmManager.cpp`、`tests/supervisor.test.mjs` |

## 顺序

1. 标题子画面：难度选择 0x44fe20（→ 机体/子季节选择）、Option 0x44c570、Replay 0x451750、Player Data 0x452330、Music Room 0x4546f0、Manual 0x4545a0、Spell Practice 0x4553d0；demo 录像触发（无输入 0x708 帧）；标题绘制链 0x44b530。
2. ANM 更高绘制模式（4+：投影/世界/条带/网格）与外部钩子：ins_508 EffectManager（0x418af0）、sprite_mapper（0x491b1c）、update/draw/delete 回调表（0x4919e8 / 0x491b58）；标题画面逐帧对照。
3. ECL VM（0x41dcb0 族）：指令集、线程/子程序、变量、与 ANM 绑定。
4. 敌机 / 子弹 / 激光 / 道具管理器；碰撞；分数/擦弹。
5. 玩家与 4 机体 × 4 子季节（pl00–pl03 + .sht）；季节系统（Season gauge、Release）。
6. Boss、符卡、符卡练习；STD 背景场景。
7. 标题 / 菜单 / Option / Key Config / Music Room / Result / Player Data；MSG 对话；Ending / Staff。
8. Replay 录制与回放（`th16_udXXXX.rpy`）、score / cfg 文件互通。
9. 音频：thbgm.fmt 循环点、音效表。
10. 全关卡 oracle 对拍：原版录像在移植版回放逐帧状态一致；关键帧像素对照。
11. 性能：60fps、单帧 < 8ms、WASI 3600 帧 < 10s。
12. 登记（workspace.json、runtime-builds.json、product-catalog、docs/migration、TOUHOU_PROGRESS、ports/README、WORKSPACE）、ADAPTATION.md 验收记录、PR。

每一步：先 oracle，再实现，再 `node --test`；AddressMap.hpp 同步登记。
