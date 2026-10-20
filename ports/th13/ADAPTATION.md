# th13 适配验收记录

按仓库适配流程登记验收结果；每轮验收引用 `artifacts/validation/source-builds/` 中的构建记录。

## 构建模式

| 模式 | 命令 | 状态 |
|---|---|---|
| 网页运行时（sdl3） | `node portable/build.mjs` | 骨架已通 |
| WASI 对照 harness | `node portable/build-harness.mjs` | 骨架已通 |

## 运行验收记录

（登记模板：模式 / 工具链 / 参数 / 源身份，附构建记录路径）
