# th13 开发指南

## 总体架构

- **行为还原**：原版 th13.exe（v1.00c）经 Ghidra headless 反编译导出函数候选与调用图；还原后的 C++ 落在 `web/cpp/game/`，按子系统推进（RNG → 帧循环 → ANM VM → ECL VM → 敌/弹/道具 → 自机 → 神灵/灵界系统）。
- **双目标同源**：同一份 C++ 编译两次——
  1. 网页运行时：Emscripten + SDL3（`portable/build.mjs` → `web/artifacts/sdl3/`）；
  2. WASI 无头 harness：wasi-sdk（`portable/build-harness.mjs` → `web/artifacts/wasi/`），供 oracle 对照与 CI。
- **Oracle 对照**：Unicorn 无头模拟原版 EXE（桩掉 Win32/D3D），以录像驱动逐帧导出状态池，与 harness 输出逐字段比对，首个分歧即回归定位点。原版二进制与 oracle 轨迹只在开发期使用，不进网页构建。
- **地址锚点**：`web/cpp/rebuild/AddressMap.hpp` 登记每个子系统的原版入口，供 oracle 与回归脚本引用。

## 统一容器环境

全工作流在标准容器内进行（本地 podman 与 GitHub Actions 共用 `container/Dockerfile`，多阶段：base → tools → dev → ci）：

```sh
make image   # 构建开发镜像（Node 22 / Emscripten 6.0.9 / wasi-sdk 34 / Ghidra 12.1.3 / Temurin 21）
make shell   # 进入开发容器（工作区挂载 /work）
make exec CMD='node portable/build.mjs'   # 单命令执行（在 eagler-touhou/ports/th13 下）
```

工作区顶层的 `games/`（原版资源）与 `artifacts/`（大体积产物）不入库；thtk 以临时克隆方式构建、用后清理（`make thtk` / `make thtk-clean`）。

## 原版资源准备

```sh
make extract   # 解包 th13ogg.7z → games/th13/，生成 SHA256 清单与版本判定
```

原版目录包含 `th13.exe`、`th13.dat`、`thbgmogg.dat`（OGG 音乐版）。资源提取（ECL/ANM/STD/msg dump）由 thtk 完成，产物落 `web/reference/`，格式版本记录在 `docs/ASSETS.md`。

## 行为还原节奏

每个子系统三重门：Ghidra 参考 → oracle 比对绿灯 → `node --test`。恢复顺序：

1. RNG 与帧计时
2. ANM 虚拟机（动画脚本）
3. ECL 虚拟机（敌机脚本）
4. 敌人 / 弹幕 / 道具管理器
5. 自机
6. th13 特有：神灵收集、灵界 Trance、Overdrive
7. Boss / 符卡 / 全 Stage 通关

## 测试

```sh
node --test tests/           # 仓库内单元测试
make exec CMD='node portable/build-harness.mjs'   # WASI harness
```
