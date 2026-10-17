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

## TH06/TH07 的 Web 音频缓冲

两作在 `src/SoundPlayer.cpp` 打开 Web 音频设备前，用 SDL 官方 `SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES` 请求 1024 帧。当前锁定的 SDL 3.4.12 Emscripten 后端会加倍该值，得到 2048 帧的 ScriptProcessor 回调缓冲。PCM 队列低/高水位为 2048/3072 帧，补充时每次最多混合三个 1024 帧片段，避免新音效等待过深的已混合队列，也避免依赖后续多个呈现帧才能补回安全水位。SDL 子模块可以使用官方提交，不需要修改其音频后端源码。

此设置只在 Emscripten 构建生效。升级 SDL 或音频后端时，需要检查实际设备帧数、浏览器节点缓冲大小与播放回调，而不是仅确认编译通过；不同后端不保证按请求值分配缓冲。当前配置不能直接按 2048 传入，否则这个后端会再次加倍为 4096。这些数值是两作参数，不是跨作品默认值。缓冲配置验证不替代完整游戏音频、失焦恢复或长时间负载验收。

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

收集器按实际提供的 `--<game>-build` 和 `--<game>-multiplayer-build` 登记变体，每个选中作品至少提供一种，产品能力限定可接受变体。普通-only、多人-only 和混合 Release 均可收集及由 Host 组装；同作双变体仍校验 DATA 布局一致。多人-only 示例：

```text
node scripts/package-runtime-release.mjs --games=th08 --th08-multiplayer-build="../../dist/runtime/th08/multiplayer" --output="../../dist/runtime-release-th08-multiplayer"
```

Site Manifest v2 允许只有多人入口，旧 v1 仍要求普通入口；旧 Host 响应过滤缺少普通 Runtime 的作品，不把多人执行文件冒充普通入口。清单/Host 行为通过不代表所选真实作品已经启动或完成多人长局。

五作 SDL 图形与 MotionTrack 由各作转发头消费 common 四头文件 backport，来源和许可见[公共库维护](../../eagler-common/docs/MAINTENANCE.md)。GLSL 原字符串相同，内部结构布局已扩展，须重新编译消费者，不与旧对象文件混用或宣称 ABI 完全等价。

不得把源码字符串中出现协议名称当作协议实现证明。文件清单、WASM 导出检查、协议行为测试与真实启动验证各自回答不同问题。
