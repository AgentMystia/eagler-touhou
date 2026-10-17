# 在 Web 启动器中接入新游戏

本页从已经准备的游戏 Runtime 开始，说明前端、资源与站点需要修改哪些声明。游戏源码的平台移植、输入/音频和 shell 实现见[根目录适配教程](../../../docs/ADAPTING_A_GAME.md)，消息含义见[Runtime 协议](../../../docs/GAME_ADAPTER_CONTRACT.md)。

以下路径在注明根目录时相对项目根；其余相对 `launcher/web/`。

## 1. 登记产品

在根目录 `contracts/src/product-catalog.mts` 的 `PRODUCT_GAMES` 添加作品。这个声明由 Web、服务和工具共同消费，保持一个入口。

| 字段 | 需要确定的内容 |
| --- | --- |
| `number`、`title`、`subtitle`、`cardArtwork` | 作品身份和目录呈现 |
| `runtime`、`runtimeFileLayout`、`runtimeAssets`、`requiredShared` | 默认入口和 Runtime 闭合文件形态 |
| `storage.saveRoot`、`scoreFile`、`configFiles` | 用户文件命名和根目录 |
| `replay.prefix` | 本作 Replay 命名空间 |
| `dataProvider`、`package.dataFileId`、`dataTarget` | DATA 提供方式、身份与挂载目标 |
| `package.musicSourceDirectories`、`musicMounts` | 音乐准备位置与 Runtime 目标 |
| `package.rawDataImport.fileNames` | 可直接导入的原版 DATA 文件名，如适用 |
| `features`、`musicCapabilities.midi` | 实际可选能力上限 |
| `support.sourceRepository` | 来源和支持入口 |
| `multiplayerRuntime`、`multiplayer` | 实际多人变体及席位、难度、角色组合、时序和观战声明 |

目录 Runtime 的 required 文件必须对应真实产物。不要复制其他作品的保存路径、Replay 前缀、角色顺序或难度范围。内容哈希、发布 URL 和安装代次由构建产生，不写在静态产品表里。

多人 loadout 的 `labelKey` 需要两种界面语言都有文本，难度和角色/机体上限从声明表取得。TH09 对战卡片是专用视图，其他作品继续使用自己的目录呈现。

在项目根可检查已有作品的登记结果：

```text
npm run adapter:inspect --workspace @eagler/web -- --game=th15
```

当前报告 schema 为 `eagler-touhou/adapter-contract-report/2`，展示 product、adapter、content 和 protocol；配置语义在 `protocol.configure`。报告用于核对声明，不自动给出构建或游戏验收通过结论。

## 2. 声明原版资源和准备方式

在 `lib/content-definition.mjs` 的 `PRODUCT_CONTENT` 登记原版输入、布局和 `hostPreparation`。能复用现有准备类型时复用；新归档、音乐或语言格式需要专门的格式处理器。

资源准备不是新的产品开关。OGG 或语言转换的方法可以不同，但不能在 Launcher 启动流程里逐作猜测。不要新增 `ThNNDirectory`、`ThNNBuild` 等旧式逐作发布参数；正式输入使用 `GameDirectories`、`LanguagePackDirectories` 和已验证 Runtime Release。

浏览器存储读取必须同时接受 Package 对象的 ArrayBuffer 与 Blob 形式，大 DATA 不能假定能塞进一条 ArrayBuffer 记录。原版文件直接导入时，按 Host 声明的大小/摘要验证，再形成普通 Package 安装，不增加另一套数据库。

## 3. 交接 Runtime 构建

游戏构建入口在根目录 `config/runtime-builds.json`，由[游戏适配教程](../../../docs/ADAPTING_A_GAME.md#7-构建打包和验收)管理。前端接收实际产物，不从未明确的兄弟目录或旧缓存猜源码。

从 Web 目录收集并验证，例如已有 TH15：

```text
node scripts/package-runtime-release.mjs --games=th15 --th15-build="<实际Runtime目录>" --output="<新Release目录>"
node scripts/verify-runtime-release.mjs "<新Release目录>"
```

所选游戏可提供普通、多人或两种变体；同作双变体的数据布局必须一致。验证 Runtime 文件清单、provider、布局和能力之后再组装站点。

## 4. 让现有前端消费声明

先确认目录、设置、资源任务、存档和 Replay 管理能从声明工作。共用会话由 `app/session/` 与 `app/services/runtime/` 协调。新增游戏一般不需要复制页面或新增一套启动状态机。

遇到差异时先确定归属：

- 作品选项、保存名和能力差异放 Product Catalog。
- 原版资源格式和转换放 content definition / 格式处理器。
- 旧发布格式读取放 `compat/`。
- 当前界面的确有特殊呈现时，使用明确的组件与输入，保持会话流程共用。

将配置转为协议规范字段，不把整个 Launcher 偏好对象传给 Runtime。低层协议类型来自根 `contracts`，不要在前端复制另一份消息格式。已有 Runtime 消費端入口见[Web 架构](ARCHITECTURE.md)。

## 5. 组装资源模式

先按[自托管指南](SELF_HOSTING.md)准备真实 DATA、字体和配置，再由同一 Runtime/资源身份派生：

| 模式 | 要验证的内容 |
| --- | --- |
| hosted | 同站点资源可安装，描述文件与 DATA/音乐摘要一致 |
| external | 网页站与配套资源源站一致；真实跨源请求、重定向及 CORS 正常 |
| import | 本地 Package 或声明的原版 DATA 正常安装，不依赖远端 DATA |

日文基线、翻译包、字体和音乐各自有可用性。未安装的可选语言回退当前启动，保留用户偏好。MIDI 只对声明支持的 Runtime 开放；OGG 的 stream/full 是解码策略，`none` 是无背景音乐模式。

Runtime 代码属于 App Shell/站点，Package DATA 属于安装存储。调整入口和文件布局后检查离线资源闭包，不能用在线首屏替代离线安装后的启动。

## 6. 从界面验收

源码命令在项目根目录执行：

```text
npm run typecheck --workspace @eagler/web
npm run typecheck:tools --workspace @eagler/web
npm run test:ui:unit --workspace @eagler/web
```

有相关服务变化再运行 `npm run test:services`；构建和资源变化运行受影响的产物用例。测试入口见[Web 开发与验证](DEVELOPMENT.md)。

必须用真实构建的前端走完：目录发现作品 → 安装/导入 → 设置 → 启动和首帧 → 操作 → 保存/Replay → 退出 → 重载。再覆盖离线、更新失败保留原代次和选中的可选功能。多人还要从房间创建/加入/准备到真实 Runtime，观战要覆盖已开赛中途进入。

交接记录包含产品声明、源码与依赖提交、Runtime Release、资源输入、所测浏览器/设备及结果。游戏规则、音频、存档和完整 Replay 的验证由[游戏适配流程](../../../docs/ADAPTING_A_GAME.md)交接；前端完成不代替那些结果。
