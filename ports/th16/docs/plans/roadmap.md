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

## 顺序

1. 帧循环与计时：supervisor、场景切换、输入采样（定位 0x43adc0 调用方与主循环；启动幕已接 0x43b300 spawn）。
2. ANM 更高绘制模式（4+：投影/世界/条带/网格）与外部钩子：ins_508 EffectManager（0x418af0）、sprite_mapper（0x491b1c）、update/draw/delete 回调表（0x4919e8 / 0x491b58）；标题画面 `title.anm` 逐帧对照。
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
