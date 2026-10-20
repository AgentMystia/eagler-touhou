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
