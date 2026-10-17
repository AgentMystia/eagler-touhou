# 游戏 Runtime 适配教程

本页面向游戏源码和平台层的维护者，说明怎样把一部作品做成可由 Launcher 启动、操作和保存数据的 Runtime。前端登记、资源准备和站点接入见 [Web 新游戏接入](../launcher/web/docs/ADAPTING_A_GAME.md)。消息格式见[接入协议](GAME_ADAPTER_CONTRACT.md)，必做项与可选功能见[能力分类](ADAPTER_CAPABILITIES.md)。

## 1. 固定源码和依赖

先记录游戏源码提交、工作区修改、common gitlink 和工具链版本。游戏子模块拥有规则、完整状态、绘制和原版文件语义；common 提供实际共用的运行支持。各作已经锁定不同 common 版本，抽取通用代码时可以只引入所需适配，不必连带升级联机核心。

开始时就与 [Web 接入步骤](../launcher/web/docs/ADAPTING_A_GAME.md#1-登记产品)对齐产品 ID、保存根目录、DATA 组合和可选功能。Runtime 开发需要当前 Launcher/协议源码作为接入依据，不等游戏完成后再补一次包装。

初始化已有作品，例如：

```text
npm run dependencies -- init th09
npm run dependencies -- status
```

新增源码仓库时，在 `.gitmodules` 登记可获取的仓库，并由父仓库记录子提交。子模块提交及递归依赖必须先具备可获取的远端引用，才能交付父仓库。构建入口不创建或切换 worktree；源码目录由维护任务明确选择。

## 2. 接好游戏的平台边界

先让目标工具链编译实际游戏路径，再实现文件、图形、输入、音频与时钟适配。游戏原有菜单、关卡、RNG、碰撞、Deathbomb、Replay 和存档语义仍由作品拥有。

| 工作 | 实现和核对方法 |
| --- | --- |
| 固定步与绘制 | 逻辑步推进游戏、RNG、脚本和 Replay；额外显示刷新核对原 Draw 的状态消费顺序，见[高刷新率](playbooks/interpolation.md) |
| 图形 | 从游戏实际状态生成 GLES/WebGL 操作；保持渲染顺序、混合、纹理与亚像素位置，见[渲染与性能](playbooks/performance-rendering.md) |
| 输入 | 保留键盘、手柄和触控的逻辑语义，处理释放、失焦、多指与场景切换，见[输入与触控](playbooks/input-touch.md) |
| 音频 | 游戏决定曲目、循环、淡入淡出和音效时机，设备层负责输出和浏览器恢复，见[音频](playbooks/audio.md) |
| 文件与 Replay | 保留原版格式和路径含义；持久化写入与 Runtime 文件接口一致，见[Replay 与确定性](playbooks/replay-determinism.md) |
| 浏览器生命周期 | 处理显示尺寸、后台、AudioContext 和 WebGL context loss，见[Web Runtime](playbooks/web-runtime.md)和[移动浏览器](playbooks/mobile-browser.md) |

触控是输入来源，不应形成第二套游戏规则。自定义移动无法由原版 Replay 表达时，扩展记录必须有自己的版本和校验；可由原版输入表达的普通记录继续使用原版格式。具体交互要求见[行为约束](ADAPTER_BEHAVIOR_INVARIANTS.md)。

## 3. 选择 Runtime 和 DATA 形态

已有两种组合，定义在 Web 的 `lib/runtime-data-provider.mjs` 与 `lib/runtime-build-profiles.mjs`：

| 组合 | Runtime 文件 | DATA 进入方式 |
| --- | --- | --- |
| flat + emscripten-preload | HTML、JS、WASM | 使用 Emscripten 生成的 preload 布局，包装器提取其声明 |
| directory + retail-memory | 入口、模块、WASM 和声明的配套文件 | 父页面提供已验证的 DATA，shell 按声明布局交给游戏 |

Runtime Release 只装可分发的代码和配套运行资源。原版 DATA、BGM、私人存档和 Replay 样本单独准备。目录 Runtime 的 `runtime-files.json` 记录闭合文件集合；产品登记的 `runtimeAssets` 列出最低所需文件。

可参考 `th06/CMakeLists.txt` 的 flat 构建，以及 `th08/portable/build.mjs`、`th08/portable/package-eagler.mjs` 的目录构建。复用文件布局与接口，不复制参考作品的保存路径、角色编号或 WASM 导出名。

## 4. 实现 Runtime shell 和启动顺序

shell 负责浏览器和游戏之间的边界：取得 `runtimeEpoch`、准备 DATA 和文件系统、接受配置及资源、发送输入、启动或停止游戏、回复文件操作。游戏循环仍由游戏代码执行。

正常顺序是：

1. shell 安装消息处理器，恢复持久化文件并准备 DATA。
2. 发送 `ready`，表示可以接受命令。
3. 接受 `configure`，验证资源、音乐、语言和 Runtime 选项，回复结果。
4. 接受需要追加的 `resources`。
5. 接受 `launch`，成功开始游戏后回复。
6. 真正呈现首帧后发送 `first-frame`。
7. 运行期间报告帧和音频状态；结束时保存数据并发送 `exit`，失败发送 `error`。

`ready` 不是游戏首帧。输入命令可以不逐条 ACK，但配置、启动和文件操作需要按请求身份回复。完整消息例子、选项归属和异常规则见[接入协议](GAME_ADAPTER_CONTRACT.md)。

## 5. 实现存档和 Replay 持久化

使用产品登记的 `storage.saveRoot`、`scoreFile`、`configFiles` 和 `replay.prefix`。游戏实际偏好路径必须与 shell 挂载点一致；只改 JS 挂载目录不能修正 C++ 仍使用的旧路径。

`list/read/write/remove/sync` 只开放允许的用户文件。拒绝越界路径、非本作文件和无效字节；写入、删除及游戏保存完成后刷新持久化存储。普通和多人保存根目录各自隔离。只读回放或文件检查不能改写用户存档。

验证至少包含：修改设置并保存、记录/导入/导出 Replay、关闭并重新加载 Runtime、确认文件仍在、拒绝越界路径、旧 epoch 请求失效。文件接口正常之后再接前端管理页，见[存储测试](../launcher/web/docs/RUNTIME_STORAGE_TESTING.md)。

游戏仓库还应提供维护者使用的 Replay Verifier：corpus 记录输入来源/摘要、角色与难度、关卡路线和完成条件；quick 覆盖自带 Demo，full 覆盖有明确授权与来源的完整 Replay。比较正常固定步后的游戏状态，覆盖 RNG、对象生命周期和原版文件语义；保存失败帧与 unknown 状态。共用比较与采集工具在 `eagler-common/testkit/replay-verifier/`，作品提供自己的适配与状态表。诊断构建独立，不给正式 Runtime 协议新增命令，具体见[Replay 教程](playbooks/replay-determinism.md)。

## 6. 接入可选功能

| 功能 | 接入要求 |
| --- | --- |
| 语言与 thcrap | 保留日文基线和原版字体路径，翻译包与字体按声明选择；见[语言适配](playbooks/localization-thcrap.md) |
| thprac | 只登记真实实现的练习能力，复用 common 工具但保留作品状态钩子；见[练习模式](playbooks/thprac.md) |
| MIDI | 游戏拥有事件时序、循环和淡出；不支持 MIDI 的作品只使用 OGG/none |
| 多人 | 明确席位、难度、角色组合、种子、输入时序、快照与结束边界；见[多人](playbooks/multiplayer.md)、[回滚](playbooks/rollback.md)和[启动校准](playbooks/adonis-adaptation.md) |

多人合作和 TH09 对战有不同规则。协作的幽灵、救援和资源转移不能直接套到对战模式；同一连接内重新开始时还需界定新的游戏代次。观战能力要由真实 Runtime 数据流支持，不能只给房间显示一个旁观席。

## 7. 构建、打包和验收

在 `config/runtime-builds.json` 登记实际源码入口。TH08/TH10/TH11 的多人源码须显式传入 `--source`，不由工具猜测当前分支。具体参数见[构建教程](playbooks/runtime-build.md)。已有入口示例：

```text
npm run runtime -- th06 --emsdk "<SDK>" --source-only
npm run runtime -- th09 --emsdk "<SDK>" --build-only
```

随后用 `launcher/web/scripts/package-runtime-release.mjs` 收集实际普通/多人变体，用 `verify-runtime-release.mjs` 验证文件、资源布局和能力。所选作品可以只有普通、只有多人或同时有两者；同作双变体需要相同 DATA 布局。

验收按实际行为记录：

- 实际游戏标题、关卡、暂停、重新开始和退出。
- 键盘、手柄、触控与失焦后的释放。
- BGM/SFX、无音乐、切曲及前后台恢复。
- 存档、设置、Replay 及重载持久化。
- 正常逻辑步、额外绘制与 Replay 状态对照。
- 启用的语言、练习或多人功能各自完整路径。

Runtime 编译和包校验通过后，仍要把真实资源交给 [Web 接入流程](../launcher/web/docs/ADAPTING_A_GAME.md)，完成安装、启动、离线与更新验证。源码提交、依赖、构建产物和实际运行环境一起保存；测试方法见[验证](playbooks/verification.md)。
