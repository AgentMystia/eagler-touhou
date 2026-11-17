# th16 反编译分区

导出：`node web/scripts/cpp/analyze.mjs` → `artifacts/th16/decomp/jp/`（`functions/<addr>.c/.asm`、`functions.json`（含调用图）、`imports.json`、`strings.json`、`blocks.json`）。Ghidra 12.1.3 headless，`ExportTh16.java` 后置脚本。

## 规模

| 分区 | 函数数 | 说明 |
| --- | --- | --- |
| 游戏代码 0x401000–0x46ffff | 1006（正文 431869 字节） | 全部可反编译 |
| CRT / 运行库 ≥ 0x470000 | 755 | 不还原，按语义替换（`operator new`、`memcpy`、`sprintf`、SEH） |
| thunk | 11 | — |

按 16 KiB 段的密度：0x40c000（67）、0x430000（69）、0x418000（64）、0x43c000（62）、0x46c000（58）为热区，对应 ECL 指令族、玩家/机体、标题菜单、ANM 管理器。

## 已还原锚点

| 原版地址 | 子系统 | C++ |
| --- | --- | --- |
| 0x402b70 / 0x402be0 | RNG 16/32 位（含临界区包装，标志 DAT_004c10b6） | `game/Rng.hpp` |
| 0x402220 | 资源加解密（奇偶位置分段，key+=step） | `game/ResourceCrypt.cpp` |
| 0x4572e0 / 0x4574b0 / 0x457120 | THA1 头部/目录/条目读取 | `game/Archive.cpp` |
| 0x457f00 | LZSS（13 位偏移 / 4 位长度 +3，字典 8192 起点 1） | `game/Lzss.cpp` |
| 0x46cdd0 / 0x46d0c0 / 0x46cf80 | ANM bank 装载 / entry 校验 / THTX 解码 | `game/AnmResource.cpp` |
| 0x46a3a0 / 0x46d1c0 / 0x46d3b0 / 0x46d8a0 | AnmManager 构造（0x1fff VM 池）/ 装载步（精灵表、模板预跑 `-1:` 块）/ 0x44 精灵记录 / uv 归一化 | `game/AnmManager.cpp AnmManager::load` |
| 0x45f980 | ANM VM 指令解释：低表 −1..209（0x462d8c/0x462cb8）、高表 301..613（0x462fa8/0x462e64），变量 0x45f610/0x45f2d0 | `game/AnmVm.cpp AnmInterpreter::run` |
| 0x4090f0 / 0x407a50 / 0x45f020 / 0x407b20 | VM 复位 / 尾部复位 / 模板初始化 / 模板拷贝 | `game/AnmManager.cpp` |
| 0x406190 / 0x406490 / 0x40d490 | 计时器推进（按 game_speed 累加）/ 设值 / 回退 | `game/AnmVm.cpp Timer` |
| 0x463b30 / 0x4033f0 / 0x4630f0 | 插值推进（模式 7 速度、0x11 加速度、8 Hermite、其余 31 种缓动）/ 缓动曲线 / 帧末速度与 uv 滚动 | `game/AnmVm.cpp` |
| 0x406380 / 0x46ed60 / 0x46eea0 | spawn / 子 VM（ins_500–506，列表位选择）/ 分离 VM（ins_507） | `game/AnmManager.cpp` |
| 0x46f600 / 0x46eab0 / 0x46e660 | 池 LIFO 分配 / 回收 / 递归标记删除 | `game/AnmManager.cpp` |
| 0x46e7d0 / 0x46e890 / 0x46e940 / 0x46ea00 / 0x46e340 / 0x46e490 / 0x46efa0 | 世界/UI 链表插入、逐帧 tick（layer 绘制链 +0x1c6fc30）、按 id 查找 | `game/AnmManager.cpp` |
| 0x407cb0 | AsciiManager：`ascii*.anm` 进槽 2；ins_300 负下标回退精灵 0x102 | `game/AnmManager.hpp kAsciiSlot` |
| 0x468490 / 0x46e750 | 按 VM / 按 layer 绘制；mode 0..3 为屏幕四边形 | `game/AnmRenderer.cpp` |
| 0x465c40 / 0x4660b0 / 0x465280 | 轴对齐角点 / 旋转角点 / 像素对齐·UV·颜色提交 | `game/AnmRenderer.cpp` |
| 0x406c40 / 0x406a70 | 世界位置（pos+offset+pos2）与窗口缩放 / root 链 | `game/AnmRenderer.cpp` |
| 0x43adc0 / 0x43b300 / 0x43b290 | 启动幕：装载 `sig.anm`；绘制链 0x44 帧计数；装载完成后请求 Title=4 | `game/GameSession.cpp` / `game/Supervisor.hpp` |
| 0x401730 / 0x401300 / 0x4013b0 / 0x401460 / 0x4015a0 / 0x4017a0 | Chain 条目 0x28 字节（prio/flags/cb/init/ctx）；更新链与绘制链按优先级插入、遍历返回码 | `game/Chain.cpp` |
| 0x45a8a0 | 帧计时：逻辑时间按 1/60 追到墙钟，再跑更新链、按跳帧计数跑绘制链 | `game/FrameClock.hpp` |
| 0x401d50 / 0x4018e0 / 0x405580 | 键盘位布局（Z=1 X=2 Shift=8 方向 0x10..0x80 Esc=0x100 Ctrl=0x200 Q=0x10000 S=0x20000 P=0x40000 Enter=0x80000）、手柄合并、边沿/连发 | `game/Input.hpp` |
| 0x44af80 / 0x44b5f0 / 0x44a700 / 0x44a680 / 0x44a790 / 0x44a650 / 0x44a560 / 0x44a5e0 | 标题更新：主菜单 case 0/1/2/4（装饰、Timer 0x78/0x82/0x13、光标、确定 0x80001 / 取消 0x102、画面表）；子项查找/中断/收起；画面与子状态切换 | `game/TitleScene.cpp` |
| 0x44fe20 / 0x4502c0 / 0x450af0 / 0x450ef0 | 难度 / 机体 / 子季节 / 练习关卡画面（VM 表 +0x2c0 按脚本号索引；面板 0x7e·0x7f / 0x96·0x98 / 0x97·0x99 / 0x71；季节 case 3 请求场景 7） | `game/TitleScene.cpp` |
| 0x449190 | 模式位（DAT_004a5bec bit4-5）与 Replay 槽复位 | `game/GameGlobals.hpp` |
| 0x402e70 / 0x418720 / 0x440c00 / 0x402de0 / 0x402e20 | MenuCursor：移动（wrap + 跳禁用）、设置、禁用、压栈、出栈 | `game/MenuCursor.hpp` |
| 0x46f340 / 0x46f3b0 | 清 flags_lo bit1（显示）并递归子树 | `game/TitleScene.cpp` |
| 0x46f510 / 0x46f0b0 / 0x46f130 | ANM 子树按脚本号查找；中断（含子 VM）；中断并立即 run | `game/AnmManager.cpp` |
| 0x467410 / 0x46b900 / 0x464dd0 / 0x464f10 / 0x45ba80 | ANM mode 8：世界矩阵 × 相机投影、±128 单位四边形锚点变体、父链旋转累加、混合/采样/寻址状态、设备缺省状态 | `game/AnmRenderer.cpp`、`game/AnmVm.cpp`、`sdl/Application.cpp` |
| 0x43cb10 / 0x43c780 / 0x43c710 | 相机组初始化（UI 相机 0x4c1560）、LookAtLH/PerspectiveFovLH 建矩阵、套用 view/proj/视口 | `game/AnmRenderer.hpp` AnmCamera |
| 0x44ad20 | 标题析构：摘两条链、卸载槽 0x10/0x11、落叶发射器（+0x73c）interrupt 1 | `game/GameSession.cpp` |
| 0x43ba40 / 0x43b3d0 / 0x43ce10 | Supervisor 注册 / 每帧泵 / 场景切换（+0x6f0/+0x6f4，DAT_004c17c4） | `game/Supervisor.hpp` |
| 0x401d50 / 0x405580 | 输入采样与边沿 | `game/Input.hpp` |
| 0x44aee0 / 0x44abc0 / 0x44ac70 | 标题创建；线程等启动幕 0xb4 帧（或 DAT_004c1800&0x180）后卸 sig、装 title*.anm、注册更新链 6 / 绘制链 0x45 | `game/GameSession.cpp` |
| 0x46f270 / 0x46d770 | 按槽杀 VM / 卸 bank | `game/AnmManager.cpp` |

ANM VM 的还原证据：`tests/anm-vm.test.mjs` 以 `tools/oracle/anm-oracle.py`（Unicorn 直跑上述原版函数）为 oracle，对 54 个 ANM 文件 1887 个脚本各 60 帧的 VM 结构体（0x568 字节纯数据区）与 36 条 layer 绘制链逐字节一致；wasm32 构建与本机构建输出逐字节相同。绘制几何：`tests/anm-draw.test.mjs` 锁定 `sig.anm` 稳态四边形（512×480 + 128×480 @ layer 26，UV v=0.9375，淡入至白）。场景时序：`tests/supervisor.test.mjs`。

## 已定位、待还原

| 原版地址 | 含义 |
| --- | --- |
| 0x43adc0（其余步骤） | 启动幕后续：`text.anm` → `../../bgm/thbgm.fmt` → `thbgm.dat` → score 对象（0x4497e0）；目前只完成 sig 上屏与向 Title 切换 |
| 0x44c570 / 0x451750 / 0x452330 / 0x4546f0 / 0x4545a0 / 0x4553d0 | 标题余项（Option / Replay / Player Data / Music Room / Manual / Spell Practice）；0x44af80 开头的 demo 录像触发；0x44a850 / 0x44a8e0 / 0x44a800 存档判定；0x41a390 + 0x418ba0 过场；0x43c470 BGM 淡出 |
| 0x46d020 | AnmManager::load_file(slot, name)（同步等待装载线程） |
| 0x418af0 / 0x405700 | EffectManager（DAT_004a6db8）：ins_508 按效果种类表 DAT_004a2250 挂 update/draw 回调；种类 1 的 update 回调维护环形顶点缓冲并生成 `effect.anm` 0x99。`effect.anm` 155–196 依赖它，暂不在 ANM golden 内 |
| 0x491b1c 表 | ins_300/301 的 sprite_mapper 回调（VM +0x5dc 非零时由玩家/敌机子系统重映射精灵号） |
| 0x41dcb0 | ECL 指令分发 |
| 0x44ac70 | 标题场景初始化 |
| 0x4497e0 / 0x4403c5 / 0x43b480 / 0x43aeae | score 文件 / replay 文件 / ver 文件 / thbgm.fmt |

未定位：敌机/子弹/激光/道具管理器、玩家与机体、季节系统（Season gauge / Release）。Supervisor、Chain 表、帧计时、输入、标题主菜单与难度/机体/季节/关卡画面已还原；Game 场景（7）与标题余项待接。全表见 `web/cpp/rebuild/AddressMap.hpp`。

## 与 th15 的关系

th16 是 th15 引擎的直接后继：THA1 密钥表、RNG 算法、ANM 版本 8 一致；差异是 RNG 外层临界区、SSE2 标量单精度编译、季节系统与 4 机体 × 4 子季节。th15 还原件可作为阅读参照，但每个函数仍以 th16 的 `.c/.asm` 为准。
