# 工程架构

## 组件职责

工程由项目工作区、Web 启动器、共享协议、联机服务和游戏 Runtime 组成。源码与 Git 依赖的入口在仓库根目录，没有 `ports/` 中间层。

| 组件 | 拥有的状态与产物 | 对外边界 |
| --- | --- | --- |
| `tools/`、`config/` | 构建参数、依赖位置、受管输出目录 | 调用各作已声明的构建入口；不改游戏规则 |
| `launcher/web/app/` | 用户界面、导航、设置、一次启动会话、反馈 | 调用应用模型及 Runtime 服务 |
| `launcher/web/src/launcher/` | 资源安装、浏览器存储、协议适配等非 React 逻辑 | 被 UI 与工具按实际用途使用 |
| `contracts/src/` | 产品登记、Runtime 消息、清单、资源代次、房间与 Replay 数据契约 | 由 Web、Node 工具与服务共同消费 |
| `services/netplay/` | 房间成员、信令、中继和遥测 | WebSocket/HTTP 与共享协议 |
| 各游戏子模块 | 游戏状态、固定逻辑步、绘制、原生文件语义 | Runtime 消息与资源接口；产出游戏代码 |
| `eagler-common/` | 多游戏公共 C++ 支持、浏览器桥接、测试工具基础 | C++/浏览器适配接口及版本化 testkit |

Web 状态的具体拥有者和离开页面的处理见 [Web 架构](../launcher/web/docs/ARCHITECTURE.md)。

## 为什么同时保留 common 和 contracts

`eagler-common` 包含 C++ 联机核心、输入/回滚/Replay 支持，以及供游戏使用的浏览器桥接。`contracts` 包含 TypeScript 数据格式和运行消息契约，实际消费者包括 Web UI、站点打包器和独立联机服务。两者的语言、产物和消费者不同，不能因为名称都像“公共层”就合并。

浏览器私有的来源读取模型已留在 Web 内部。新增公共接口须指出现有消费者及稳定的协议边界；单一 UI 组件的内部状态不进入根 contracts。

根目录的 common 也不覆盖各游戏锁定的 `third_party/eagler-common`。这些锁定值不同，游戏编译使用各自声明的版本。`npm run dependencies -- status` 同时显示根依赖与游戏实际 common 版本。升级按消费者验证，避免一次性把不同演进阶段的游戏指向同一提交。

## 源码、资源与交付

1. 游戏构建产生普通或多人 Runtime；游戏仓库不再保存独立 Web 启动器副本。
2. Runtime 打包验证文件集合、资源布局和代次身份，产生不带原版 DATA 的 Runtime Release。
3. Web 构建产生正式 React 静态入口与资源图。
4. 站点组装读取显式的游戏目录、字体、语言、音乐和 Runtime Release，生成站点清单。
5. 发布候选保存站点、Runtime、校验清单及来源信息。部署和上传是后续独立操作。

文件哈希用于发现损坏、混用版本及不完整交付；它不证明游戏逻辑正确。清单中的能力也是运行时声明，必须另附对应行为证据。

## 跨平台边界

通用入口采用 Node.js、Python、CMake/CTest。Emscripten、字体、原版资源与特殊源码从参数或忽略的本地配置传入。Windows 的 npm 启动、浏览器打开和目录连接替换保留小型适配。

目前保留的项目 PowerShell 是 common 的 Windows D3D8 Present 观察器构建，工作依赖 Windows 原生环境；它不承担通用发布。Linux/macOS 的 CI 已配置，是否执行成功以实际工作流结果为准，本轮本机结果不替代跨系统验收。

## 扩展一部作品

游戏维护者按[Runtime 适配教程](ADAPTING_A_GAME.md)实现平台层、[接入协议](GAME_ADAPTER_CONTRACT.md)、存档/Replay 和所选可选功能，再交付实际 Runtime Release。

Web 维护者按[前端接入教程](../launcher/web/docs/ADAPTING_A_GAME.md)登记产品与原版资源，复用目录、安装、设置和会话流程，并完成真实界面的安装、启动与重载。产品和消息类型共享根 `contracts`，不因此把前端具体操作放进根架构说明。

开发用诊断导出、Presentation Lab 和 Replay Verifier 是独立验证边界。诊断核心通过比较不能直接代表浏览器绘制、音乐、存储或完整游戏验收。
