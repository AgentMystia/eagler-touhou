# th13 适配验收记录

按仓库适配流程登记验收结果；每轮验收引用 `artifacts/validation/source-builds/` 中的构建记录。

## 构建模式

| 模式 | 命令 | 状态 |
|---|---|---|
| 网页运行时（sdl3） | `node portable/build.mjs` | 骨架已通（浏览器主循环启动验证） |
| WASI 对照 harness | `node portable/build-harness.mjs` | 骨架已通（node:wasi 运行验证） |

## 运行验收记录

- 2026-10-21 · 初始化轮 · 统一容器（`ports/th13/container`，podman/CI 共用）
  - 网页运行时：SDL3 主循环在浏览器安装运行（`web/sdl-runtime/th13.html`）。
  - RNG 行为闭环：Unicorn oracle 直调原版 `0x47BBA2`（CRT `_rand()`）得 MSVC srand(1) 标准序列；WASI harness 同种子输出逐值一致（`41 18467 6334 26500 19169 15724 11478 29358`）。
  - 单元测试：`node --test tests/*.test.mjs` 2/2 通过。
- 2026-10-21 · 第 1 轮 · ECL 虚拟机（行为规格 `docs/plans/ecl-vm.md`，命名对齐 th15 五模块）
  - 模块：`web/cpp/game/{Types,Interpolation,EclResource,EclContext,EclThreads,EclFile}` + `EclInterpreter.cpp`（基础指令 0–93 内联，>93 经 EclCommands 回调）；Rng 补 `GameRandom`（原版 0x472c90 游戏侧 16 位 LCG，反汇编逐指令还原）。
  - 取参语义勘误：容器内 capstone 直读 0x478e60/0x478ee0 反汇编，整型/浮点均 ≥0 局部、[-100,-1] 表达式栈（×8 步长，-1 为栈顶）、<-100 全局（浮点判定常量 -100.0/-1.0/8.0 在 0x4ae900/0x4ae908/0x4ae910）；规格 §1.3 整型行与此相反，以代码为准。
  - 单元测试：容器内 g++ 三套件 18/18 通过（`tests/ecl/run.sh`，真实 st01.ecl 字节固定用例 + 全 59 子程序冒烟）；`node --test tests/*.test.mjs` 3/3 通过（含 C++ 套件包装）；WASI harness 构建通过。
