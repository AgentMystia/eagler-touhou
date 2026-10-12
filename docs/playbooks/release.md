# 打包与发布

## 准备候选

游戏构建、Runtime Release 和 Web 前端是可独立检查的输入。先运行对应检查，再准备原版资源与明确的功能配置。入口不会替维护者推送、创建公开发布或部署。

在 `launcher/web` 中：

```text
node scripts/release.mjs --input="<release-input.json>" --output="<新的候选目录>"
node scripts/build-release-assets.mjs --candidate="<候选目录>" --output="<新的附件目录>" --unicode-font="<字体>" --font-license="<许可文本>"
```

正式输入 schema 为 `eagler-touhou/release-input/1`。根 `games` 选择作品；`prepare` 使用[资源 playbook](resources.md)中的显式输入。候选自有输出目录、Python 缓存等不能覆盖。正式发布不会从临时源码分支即时编译游戏。

附件工具先验证候选，随后生成 `EaglerTouhou-SelfHost.zip`、`EaglerTouhou-Runtime.zip`、`SHA256SUMS.txt` 和附件说明。它只写本地产物。

## 独立自托管包

不需要完整正式候选时，可以直接从已验证的 Runtime Release 生成自托管目录和 ZIP：

```text
node scripts/build-self-host-bundle.mjs --runtime-release="<Runtime Release>" --output="<新目录>" --unicode-font="<字体>" --font-license="<许可文本>" --archive="<新ZIP>"
```

自托管包包含预构建 Web、操作工具及自己的依赖锁，不包含游戏原版 DATA。首次安装依赖需要网络或完整缓存。验收必须在解压后的独立目录执行，不能借用开发仓库依赖。

## 内部运营交付

`tools/maintainer/build-operator-delivery.mjs` 必须显式传入 `--profile`。示例是配置结构，不是自动选择的正式发布配置。功能必须能由候选的真实 Runtime、语言与音乐清单支持；迁移来源和外部资源地址须按目标站点另行指定。

站点目录替换保留旧目录恢复路径。Windows 目录连接切换可能有短暂空隙，不宣称无间断原子替换。实际上传和部署前应审核候选、目标、公开文案及回退方式。

## 本轮证据边界

已执行真实 TH15 的 Node 组装、独立 ZIP 解压及自托管重建；未提供 BGM，不代表完整音频发布验收。ZIP 的 Unicode 路径、空目录和 CRC 有专项验证。完整正式发布候选、远端上传、生产切换与所有作品的运营配置未在本轮执行。
