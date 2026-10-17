# 项目文档

根文档维护游戏 Runtime 的适配教程、接入协议、共同能力和实现经验，以及源码构建和交付流程。前端新增游戏、界面、资源安装与站点运维在 [Web 文档](../launcher/web/docs/README.md)。

| 需要做什么 | 入口 |
| --- | --- |
| 适配一部游戏的 Runtime | [游戏适配教程](ADAPTING_A_GAME.md) |
| 实现与 Launcher 通信的接口 | [Runtime 接入协议](GAME_ADAPTER_CONTRACT.md) |
| 判断必做项和可选功能 | [能力分类](ADAPTER_CAPABILITIES.md)、[行为约束](ADAPTER_BEHAVIOR_INVARIANTS.md) |
| 查输入、音频、渲染、回滚等实现方法 | [专题教程索引](playbooks/README.md) |
| 在前端登记游戏并跑通安装与启动 | [Web 新游戏接入](../launcher/web/docs/ADAPTING_A_GAME.md) |
| 了解组件和依赖 | [工程架构](ARCHITECTURE.md) |
| 安装环境、初始化源码、选择检查 | [开发指南](DEVELOPMENT.md) |
| 确认功能状态与证据 | [能力进度](TOUHOU_PROGRESS.md) |
| 查阅本次重构的问题和验证记录 | [工程审计](ENGINEERING_AUDIT.md) |
| 构建普通或多人 Runtime | [Runtime 构建](playbooks/runtime-build.md) |
| 选择验证方法、添加回归 | [验证](playbooks/verification.md) |
| 修改输入、Replay、回滚或绘制 | [输入与确定性](playbooks/input-replay.md) |
| 调整房间、传输和多人集成 | [多人联机](playbooks/multiplayer.md) |
| 准备原版资源、字体、音乐、语言包 | [资源与语言](playbooks/resources.md) |
| 打包、交付和发布候选 | [发布](playbooks/release.md) |
| 识别源码、编译、候选和验证输出 | [产物说明](ARTIFACTS.md) |

教程记录实现方法、适用范围、代码入口和验证步骤。工程审计单独记录此次重构，不作为适配手册。固定环境的历史案例须标明范围；聊天记录、个人机器路径和一次性本机任务日志不进入操作教程。
