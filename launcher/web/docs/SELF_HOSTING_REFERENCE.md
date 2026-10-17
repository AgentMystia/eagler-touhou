# 自托管参考

本页定义生成站点及 Host 配置的行为。快速开始见[自托管指南](SELF_HOSTING.md)。

## 输入、缓存和产物

运营者维护 `eagler-touhou.config.json`、`games/`、`shared/`、可选 `artwork/` 和 `runtime-release/`。`.cache/python-environments/`、`.cache/tools/`、`.cache/generated/` 保存可重建的工具及中间产物。`dist/site/`、`dist/import-site/`、`dist/import/` 和派生的 `dist/external-site/` 是输出。

修改输入后重新生成；不要直接修改 DATA、语言包或音乐产物后继续使用旧清单。

## 配置

`eagler-touhou.config.json` 的 schema 是 `eagler-touhou/host-config/1`。

| 字段 | 说明 |
| --- | --- |
| `games.<game>.languages` | 非空且不重复的语言列表；`ja`、`lang_zh-hans`、`lang_en`，受实际 Runtime 能力限制 |
| `games.<game>.thprac` | 布尔值，启用时要求 Runtime 支持练习 |
| `netplay.relay` | 可选 `ws://` 或 `wss://` 服务地址；公共 HTTPS 站点使用可访问的 WSS |
| `externalImportSource.url` | 可选 HTTPS 手动下载链接 |
| `externalImportSource.hint` | 该链接的说明 |

未配置 Relay 时不提供可用的联机服务入口；单人资源构建仍可运行。外部下载链接不会自动开启 external 资源模式，也不会授权静默下载第三方游戏包。TURN 密钥由服务端管理，不写入公开站点配置。

游戏集合来自 Runtime Release。`games` 只调整该集合中的功能，不能用它引用 Release 之外的游戏。

## 三种资源模式

| 模式 | 启动器和 Runtime | 游戏资源 |
| --- | --- | --- |
| hosted | 站点发布 | 同站点发布，由 Package 安装 |
| external | 网页站点发布 | 跳转到同代次 Hosted 资源源站 |
| import | 站点发布 | 用户主动选择本地 Package |

`import-only` 和 `import-partial` 只保留历史读取兼容。网络失败不会自动改写部署模式或提交一个未完成的资源代次。

`npm run import -- --rebuild-hosted-base` 可强制重建导入模式所用的 Hosted 基础。普通导入构建在输入匹配时复用已验证基础。

## 依赖

源码使用根 npm lock；独立自托管包使用包内 lock。Node 启动器会在必要时执行 npm ci，且检查要求的依赖版本。Windows 的 npm 通过实际 npm-cli.js 调用，路径包含空格也不需要拼接 shell 命令。

Python 环境由 Host 创建，支持 CPython 3.12—3.14。`host/requirements.txt` 锁定包版本和 wheel SHA-256，安装要求 hash 匹配且只接受 wheel。环境身份包括解释器实现/版本/平台、锁和检查器内容；复用前检查安装的发行包集合及文件指纹。修改锁后会选择新的环境，不直接覆盖旧环境。

Windows 的 thtk 12 包会校验下载哈希；非 Windows 由运营者安装 `thdat` 和 `thmsg`。只选择日文不会强制安装翻译工具。原版游戏数据不会由依赖安装器下载。

## HTTP 与缓存

站点可由 nginx、Caddy、IIS、对象存储或其他服务承载，需满足可观察行为：

- 使用 HTTPS 或浏览器认可的安全上下文。
- WASM 使用 `application/wasm`；ES module 使用正确 JavaScript MIME。
- HTML、JSON 和无版本入口允许重新验证；带内容身份的资源可以长期缓存。
- CDN 缓存键保留资源身份查询参数，包括 `v`。
- 不添加阻断 iframe、WASM、音频或 Service Worker 的策略。
- 发布完整代次，保留切回前一个代次的能力。

[服务器示例](../examples/deployment/README.md)只演示配置，不安装系统、证书或防火墙。部署后运行 `verify:deployed`，并在实际浏览器验证安装与启动。

## 联机服务

参考实现位于根 `services/netplay/`，源码执行 `npm run relay`。静态自托管包与独立服务包是不同交付物。可部署兼容服务并把 WSS 地址写入配置。

服务端拥有房间、信令、WebSocket 中继和 TURN 凭据下发；客户端通过 WebRTC 或中继传输。部署后检查信令、实际双端传输及断线恢复；完整服务流程在源码仓库 `docs/playbooks/multiplayer.md` 和 `docs/playbooks/deployment-network.md`。

## Origin 迁移

浏览器存储按 Origin 隔离；从 HTTP 切到 HTTPS 或更换域名不会自动共享存档。仅在实际迁移部署中启用对应能力。

HTTP 到 HTTPS 的迁移窗口要求精确的 HTTP `/migrate.html` 仍可访问；此时不能提前用 HSTS 强制跳过旧 Origin。退役迁移能力后再切换最终 HTTPS/HSTS 配置。使用 `verify-origin-cutover.mjs` 和 `verify-hsts-cutover.mjs` 检查各自阶段，不混用两阶段配置。

## 验证等级

结构与哈希验证证明交付内部一致；浏览器安装证明当前环境可下载、提交和启动；游戏验收还需要游玩、输入、音频、存档与 Replay。发布报告必须分别记录，不能把文件校验结果写成全作品兼容承诺。
