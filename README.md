<h1 align="center"><img src="docs/assets/eagler-touhou-wordmark.svg" alt="EAGLER TOUHOU" width="312"></h1>

<p align="center">东方 Project 游戏的 Web 适配、统一启动器与多人联机工程。</p>
<p align="center">简体中文 | <a href="README.en.md">English</a></p>

本仓库保留原有历史，统一管理 Web 启动器、共享协议、联机服务，以及 TH06—TH11、TH15 的源码依赖。游戏以 WebAssembly Runtime 运行；启动器负责资源安装、设置、触控、存档和 Replay 管理。原版游戏数据需要另外提供。

**当前能力和验证范围以[项目进度](docs/TOUHOU_PROGRESS.md)为准。** 功能登记、编译成功和真实游戏验收分别记录。TH09 的 Runtime 仍明确标记为未完成；TH20 保留现有产品与预编译接入，其源码不在本轮工程审计范围内。

![启动器界面](docs/assets/readme/launcher.webp)

## 使用与部署

- 玩家使用已部署站点，可按站点能力安装资源或导入自己持有的游戏包；问题见 [FAQ](launcher/web/docs/FAQ.md)。
- 部署者见[自托管指南](launcher/web/docs/SELF_HOSTING.md)。Hosted、External、Import 共用当前 Web 前端。
- 已安装资源和已缓存 Runtime 可以用于离线运行。浏览器存储、站点版本和安全上下文仍会影响可用性，详见[缓存维护](launcher/web/docs/CACHE_AND_COMPATIBILITY.md)。
- 多人能力仅在对应 Runtime、房间服务和网络配置均可用时启用；TH15 没有多人模式。

## 开发入口

维护环境使用 Node.js 24.15 或同一主版本的后续版本，也接受 Node.js 26 及以上。Python、CMake、Ninja 和 Emscripten 按实际任务准备。

```text
npm ci --ignore-scripts
npm run doctor
npm run dev
npm run build
npm run check
```

Web 前端开发不要求先下载游戏数据。需要构建某部作品时，先初始化该依赖：

```text
npm run dependencies -- init th15
npm run runtime -- th15 --emsdk "<SDK目录>" --font-root "<字体目录>"
```

完整参数、TH06/TH07 的资源要求和多人源码选择见[开发指南](docs/DEVELOPMENT.md)及[Runtime 构建](docs/playbooks/runtime-build.md)。

## 工程布局

| 目录 | 职责 |
| --- | --- |
| [launcher/web/](launcher/web/) | 当前 Web 启动器、站点组装与部署工具 |
| [contracts/](contracts/) | Web、Node 服务和工具共同使用的协议与数据格式 |
| [services/netplay/](services/netplay/) | 房间、信令、中继和联机遥测服务 |
| `th06/`—`th11/`、`th15/` | 根目录 Git 子模块，各作产出 Runtime |
| [eagler-common/](eagler-common/) | C++ 公共运行支持、浏览器桥接和共享 testkit |
| [tools/](tools/)、[config/](config/) | 工程入口、依赖与构建声明 |
| [docs/](docs/) | 项目架构、能力进度、审计和维护 playbook |

当前整理范围是现有工程；其他平台启动器尚未实现，也没有确定外壳技术选型。

## 维护与贡献

先读[文档索引](docs/README.md)和[工程审计与改进清单](docs/ENGINEERING_AUDIT.md)。修复应说明受影响的行为、实际验证和尚未覆盖的范围。不要以源码字符串、整份 CSS 哈希或历史目标文件一致性替代行为验证。

[项目交流群](https://qm.qq.com/q/eeUrxIltug?from=tim)用于交流适配与使用问题。

## 许可证与资源

本工程自身代码采用 [GPL-3.0-or-later](LICENSE)。游戏仓库和第三方组件遵循各自许可证，见[第三方说明](THIRD_PARTY.md)。原版数据、个人存档、工具链和部署配置不属于公开源码，见[资源说明](ASSETS.md)。
