# 工程能力进度

本表记录 2026-10 工程审计及随后的整改。项目入口与游戏 gitlink 以本文件所在提交为准；状态不能由单个功能开关、旧报告或编译成功推导。后续修改应更新受影响的行和实际验证范围。

## 状态含义

- **实现存在**：可定位到当前代码，不等于已执行验收。
- **本轮通过**：按明确命令、输入和环境实际执行。
- **有限覆盖**：只证明所列子系统、字段或场景。
- **未验收/未提供**：没有本轮运行证据，或当前未登记对应能力。

本轮执行环境为 Windows、Node.js 24.15.0、Python 3.14.3，游戏构建使用实际配置的 Emscripten、CMake/Ninja。CI 已配置 Windows/Linux/macOS；远端三系统运行结果不由本机结果替代。

## 大类能力

| 能力大类 | 当前实现与本轮证据 | 尚未覆盖 | 架构与接手流程 |
| --- | --- | --- | --- |
| Runtime 与平台构建 | 七作普通源码编译通过；TH08/09/10/11/15 的目录 Runtime 打包与验证通过；TH06/07 无 DATA 和基础练习编译通过 | 所有机体/关卡游玩、Linux/macOS 原生构建和其他设备 | [架构](ARCHITECTURE.md)、[Runtime 构建](playbooks/runtime-build.md)、[构建声明](../config/runtime-builds.json) |
| 启动器工作流与会话 | 当前 React UI；设置/目录模型、导航决策、浏览器会话和 Runtime 所有者分开；类型、mounted 行为及产物测试 | 大型生命周期协调器仍有进一步拆分空间 | [Web 架构](../launcher/web/docs/ARCHITECTURE.md)、[Web 验证](../launcher/web/docs/DEVELOPMENT.md) |
| 资源安装、缓存与离线 | Package 事务、Runtime/App Shell 代次、导入与更新；真实 React/Service Worker 的 13 项 PWA 场景通过；TH15 真实资源组装及独立 ZIP 冷启动通过 | 全作品导入/升级、各浏览器容量回收和物理设备离线矩阵 | [缓存与兼容](../launcher/web/docs/CACHE_AND_COMPATIBILITY.md)、[资源流程](playbooks/resources.md) |
| 输入、存储与 Replay | 键盘/触控所有权、文件会话、存档与 Replay 管理；TH15 真实浏览器进入游戏并写入/重载存档；common 输入 Replay 编解码及原生测试 | 全作品存档互通和完整原版 Replay 对照；诊断字段不是全内存 | [输入与 Replay](playbooks/input-replay.md)、[common 格式](../eagler-common/docs/input-replay.md) |
| 呈现、性能与诊断 | 固定步/呈现边界有实现；TH08 Presentation Lab、TH10 有限适配及 common testkit 存在 | 本轮未重跑物理手机性能或原版绘制比较；TH06/07 不宣称 Lab 通过；TH10 覆盖不足时为 unknown | [游戏手册](#各作状态)、[common Lab](../eagler-common/testkit/presentation-lab/README.md)、[验证流程](playbooks/verification.md) |
| 多人游戏与网络服务 | TH06—TH11 有多人产品登记；服务独立为 `services/netplay`；C++ 会话/输入/回滚测试及独立打包 Relay 的真实 WebSocket 房间测试通过 | 未据此证明全部游戏 2/3 端、跨网、观战、重连和全关卡正确；TH08/10/11 多人源码需明确选择；TH15 无多人 | [多人流程](playbooks/multiplayer.md)、[服务](../services/netplay/)、[common 维护](../eagler-common/docs/MAINTENANCE.md) |
| 语言、音乐与练习 | 按 Product Catalog、Runtime Release 和 Host 选择生成资源；DATA 与音乐准备独立；TH06/07 基础练习编译，TH15 使用日文、声明练习能力的 Runtime 完成真实启动及无 BGM 组装 | 不等于完整上游 thcrap/thprac；各作翻译版布局、OGG/MIDI 循环和练习章节仍需分别验收 | [资源流程](playbooks/resources.md)、[集成边界](../launcher/web/integrations/RUNTIME_CONTRACT.md)、[Host 配置](../launcher/web/docs/SELF_HOSTING_REFERENCE.md) |
| 工程验证与交付 | 根 workspace、Git 依赖、Node/Python/CMake 入口、受管源码检查、不可变 contracts 构建代次、事务性 Runtime 输出和中文维护体系 | Python 完整锁定、浏览器服务构建锁恢复、普通/多人独立收集及全候选验收仍需完善 | [开发](DEVELOPMENT.md)、[发布](playbooks/release.md)、[完整审计](ENGINEERING_AUDIT.md) |

## 各作状态

| 作品 | 本轮普通构建 | Runtime/真实浏览器 | 多人 | 主要限制与接手入口 |
| --- | --- | --- | --- | --- |
| TH06 红魔乡 | 无 DATA 普通与基础练习构建通过 | 产出 HTML/JS/WASM；本轮未做真实 DATA 全流程 | 已登记；5 项原生核心测试通过不等于多人全流程 | [维护](../th06/docs/MAINTENANCE.md)、[语言适配](../th06/docs/LOCALIZATION.md) |
| TH07 妖妖梦 | 无 DATA 普通与基础练习构建通过 | 产出 HTML/JS/WASM；本轮未做真实 DATA 全流程 | 已登记；4 项原生核心测试通过不等于多人全流程 | [维护](../th07/docs/MAINTENANCE.md) |
| TH08 永夜抄 | 通过 | 目录 Runtime 打包与完整性验证通过 | 已登记；本轮未重建或运行独立多人源码 | [维护](../th08/docs/MAINTENANCE.md)、[Lab](../th08/portable/presentation-lab/README.md)、[Replay](../th08/tools/replay-verifier/README.md) |
| TH09 花映塚 | 通过 | 目录 Runtime 打包通过；仍为 `completeGame: false` | 独立对战适配，不能套用其他作品协作规则 | [维护](../th09/docs/MAINTENANCE.md)；旧子系统对照不代表当前整作完成 |
| TH10 风神录 | 通过 | 目录 Runtime 打包与完整性验证通过 | 已登记；本轮未重建或运行独立多人源码 | [维护](../th10/docs/MAINTENANCE.md)、[Lab](../th10/portable/presentation-lab/README.md)、[Replay](../th10/tools/replay-verifier/README.md) |
| TH11 地灵殿 | 通过 | 目录 Runtime 打包与完整性验证通过 | 已登记；本轮未重建或运行独立多人源码 | [维护](../th11/docs/MAINTENANCE.md)、[Replay](../th11/tools/replay-verifier/README.md) |
| TH15 绀珠传 | 通过 | 真实 DATA 的 Chromium 启动、进入游戏、触屏模拟及存档写入/重载通过；独立自托管构建通过 | 未登记、未提供 | [维护](../th15/docs/MAINTENANCE.md)、[Replay](../th15/tools/replay-verifier/README.md)；本轮无 BGM，不代表全章节或物理手机验收 |

TH20 保留现有目录和预编译产品接入，但源码不在本轮 TH06—TH11、TH15 审计范围内。本轮没有实现其他平台启动器，也没有确定 Flutter、Tauri 或原生外壳选型。

## 验证记录与重现

| 验证 | 本轮结果 | 重现入口与范围 |
| --- | --- | --- |
| Web 核心 | 受管源码与核心测试通过 | `node launcher/web/scripts/check.mjs` |
| UI 类型 / 工具类型 | 通过 | Web `typecheck`、`typecheck:tools` |
| UI 单元与 mounted | 当前完整组 1092 项通过，0 失败、0 跳过 | Web `test:ui:unit`；不包含全部真实游戏浏览器脚本 |
| UI 产物 | 六个产物测试文件分别通过 | Web `test:ui:artifact`；编译、App Shell、刷新与服务 |
| 原生 | common 32、TH06 5、TH07 4 项 CTest 通过 | 项目 `check:workspace` 中对应 CMake/CTest |
| 真实浏览器 | 当前内容页、房间快捷消息、TH15 启动/存档及 13 项 PWA 场景通过 | 对应 Web npm 浏览器入口；PWA 使用合成 Runtime |
| 分发边界 | 独立 Relay 实际房间、Unicode/空目录 ZIP、TH15 Node 组装和独立解压冷构建通过 | 服务打包、`assemble:site`、`package:self-host` |
| 发布/远端 | 未执行推送、GitHub 发布或部署 | 不能把本地验证写成生产验收 |

具体失败原因、修复及剩余项见[工程审计](ENGINEERING_AUDIT.md)。原始资源、带个人路径的运行记录和浏览器配置保存在源码外；维护文档记录可重复步骤和范围。

## common 版本

游戏实际使用自己的递归依赖。以下锁定差异仍然保留；升级时分别验证消费者，不能把根 common 较新当作已完成兼容升级。

| 消费者 | `third_party/eagler-common` 锁定 |
| --- | --- |
| TH06、TH07 | `af954fee72ebe623546be3ebca45dcff9b2b73bd` |
| TH08 | `b3071f6ab720ae396777c3ff4c200d4ece0b7c8d` |
| TH09 | `386978c8248de13b05ad390da6a3ea2cbde2e947` |
| TH10、TH15 | `b0903054d105aed27a38b4038ed9fc4991c75c19` |
| TH11 | `984a8a10834c35fe9ef86c4ffe03daf1fb61f87c` |

用 `npm run dependencies -- status` 和 `git submodule status --recursive` 核对实际工作区。Git 指针、编译身份和运行结果一起构成可接手的版本依据。
