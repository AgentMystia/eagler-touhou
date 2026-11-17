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

ANM VM 的还原证据：`tests/anm-vm.test.mjs` 以 `tools/oracle/anm-oracle.py`（Unicorn 直跑上述原版函数）为 oracle，对 54 个 ANM 文件 1887 个脚本各 60 帧的 VM 结构体（0x568 字节纯数据区）与 36 条 layer 绘制链逐字节一致；wasm32 构建与本机构建输出逐字节相同。

## 已定位、待还原

| 原版地址 | 含义 |
| --- | --- |
| 0x43adc0 | 启动幕初始化：`sig.anm` → `text.anm` → `../../bgm/thbgm.fmt` → `thbgm.dat` → score 对象（0x4497e0） |
| 0x46d020 | AnmManager::load_file(slot, name)（同步等待装载线程） |
| 0x418af0 / 0x405700 | EffectManager（DAT_004a6db8）：ins_508 按效果种类表 DAT_004a2250 挂 update/draw 回调；种类 1 的 update 回调维护环形顶点缓冲并生成 `effect.anm` 0x99。`effect.anm` 155–196 依赖它，暂不在 ANM golden 内 |
| 0x491b1c 表 | ins_300/301 的 sprite_mapper 回调（VM +0x5dc 非零时由玩家/敌机子系统重映射精灵号） |
| 0x41dcb0 | ECL 指令分发 |
| 0x44ac70 | 标题场景初始化 |
| 0x4497e0 / 0x4403c5 / 0x43b480 / 0x43aeae | score 文件 / replay 文件 / ver 文件 / thbgm.fmt |

未定位：supervisor 帧循环、敌机/子弹/激光/道具管理器、玩家与机体、季节系统（Season gauge / Release）。全表见 `web/cpp/rebuild/AddressMap.hpp`。

## 与 th15 的关系

th16 是 th15 引擎的直接后继：THA1 密钥表、RNG 算法、ANM 版本 8 一致；差异是 RNG 外层临界区、SSE2 标量单精度编译、季节系统与 4 机体 × 4 子季节。th15 还原件可作为阅读参照，但每个函数仍以 th16 的 `.c/.asm` 为准。
