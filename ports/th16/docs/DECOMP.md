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

## 已定位、待还原

| 原版地址 | 含义 |
| --- | --- |
| 0x43adc0 | 启动幕初始化：`sig.anm` → `text.anm` → `../../bgm/thbgm.fmt` → `thbgm.dat` → score 对象（0x4497e0） |
| 0x46d020 | AnmManager::load_file(slot, name) |
| 0x45f980 | ANM VM 单步更新 |
| 0x41dcb0 | ECL 指令分发 |
| 0x44ac70 | 标题场景初始化 |
| 0x4497e0 / 0x4403c5 / 0x43b480 / 0x43aeae | score 文件 / replay 文件 / ver 文件 / thbgm.fmt |

未定位：supervisor 帧循环、敌机/子弹/激光/道具管理器、玩家与机体、季节系统（Season gauge / Release）。全表见 `web/cpp/rebuild/AddressMap.hpp`。

## 与 th15 的关系

th16 是 th15 引擎的直接后继：THA1 密钥表、RNG 算法、ANM 版本 8 一致；差异是 RNG 外层临界区、SSE2 标量单精度编译、季节系统与 4 机体 × 4 子季节。th15 还原件可作为阅读参照，但每个函数仍以 th16 的 `.c/.asm` 为准。
