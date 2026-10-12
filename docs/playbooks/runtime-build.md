# Runtime 构建

## 选择源码与产物

先运行 `npm run dependencies -- status`，确认目标游戏和递归依赖完整。构建计划来自 `config/runtime-builds.json`；`--dry-run` 可查看实际命令，不执行编译。

| 游戏 | 实际源码入口 | 产物 |
| --- | --- | --- |
| TH06、TH07 | 游戏根 CMake 工程 | `build/web/<variant>/` 中的 HTML、JS、WASM |
| TH08 | `portable/build.mjs --th08`、`portable/package-eagler.mjs` | 目录 Runtime |
| TH09 | `th09_web/scripts/build-sdl.mjs --release`、`th09_web/scripts/build-eagler.mjs` | 目录 Runtime |
| TH10、TH11 | `portable/build.mjs`、`portable/package-eagler.mjs` | 目录 Runtime |
| TH15 | `th15_web/scripts/build-sdl-application.mjs --release`、`portable/package-eagler.mjs` | 目录 Runtime |

SDL 目录 Runtime 默认进入根 `dist/runtime/<game>/<variant>/`。输出目录由工程入口校验和管理，失败时保留上一份有效目录。

## 常见操作

```text
npm run runtime -- th06 --emsdk "<SDK>" --source-only
npm run runtime -- th06 --emsdk "<SDK>" --source-only --thprac
npm run runtime -- th07 --emsdk "<SDK>" --assets "<原版与字体目录>"
npm run runtime -- th15 --emsdk "<SDK>" --font-root "<字体目录>"
npm run runtime -- th09 --emsdk "<SDK>" --build-only
```

`--source-only` 仅适用于 TH06/TH07：验证无原版资源的编译，不是可直接发布的 DATA 构建。`--build-only` 适用于 SDL 入口，跳过 Runtime 打包。`--jobs N`、`--python PATH`、`--cmake PATH`、`--ninja PATH` 用于明确环境。

TH06/TH07 的练习界面使用根 `third_party/imgui` 中有来源和许可证的源码。`--imgui-root` 可覆盖它；`--runtime-extension` 才引入额外的扩展 CMake 文件。基础练习构建不代表完整上游 thprac 功能。

## 多人变体

TH06/TH07 用明确的 CMake 变体开关；TH09 有对应的打包变体。TH08/TH10/TH11 的多人构建必须显式传入 `--source`，以免把普通分支当成多人源码：

```text
npm run runtime -- th08 --variant multiplayer --source "<已准备的多人源码>" --emsdk "<SDK>" --font-root "<字体目录>"
```

入口不会自动创建或切换 worktree。应记录该源码目录的提交、工作区状态及 common 版本。本轮完成七作普通编译，不据此声称所有多人分支都经过重建或联机验收。

## Runtime Release

在 `launcher/web` 中收集已构建产物，例如 TH15：

```text
node scripts/package-runtime-release.mjs --games=th15 --th15-build="../../dist/runtime/th15/normal" --output="../../dist/runtime-release-th15"
node scripts/verify-runtime-release.mjs "../../dist/runtime-release-th15"
```

对登记了多人产品的作品，当前收集器仍要求对应 `--<game>-multiplayer-build`，并核对普通/多人数据布局。选中作品可缩小，普通与多人独立发布仍是待解耦项，见审计清单。

不得把源码字符串中出现协议名称当作协议实现证明。文件清单、WASM 导出检查、协议行为测试与真实启动验证各自回答不同问题。
