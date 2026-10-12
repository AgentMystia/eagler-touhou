# 项目文档

项目文档说明跨游戏、公共库、协议与交付的边界。Web 启动器的状态、导航和站点维护放在 [launcher/web/docs](../launcher/web/docs/README.md)。

| 需要做什么 | 入口 |
| --- | --- |
| 了解组件和依赖 | [工程架构](ARCHITECTURE.md) |
| 安装环境、初始化源码、选择检查 | [开发指南](DEVELOPMENT.md) |
| 确认功能状态与证据 | [能力进度](TOUHOU_PROGRESS.md) |
| 查看本轮问题、整改和剩余工作 | [工程审计](ENGINEERING_AUDIT.md) |
| 构建普通或多人 Runtime | [Runtime 构建](playbooks/runtime-build.md) |
| 选择验证方法、添加回归 | [验证](playbooks/verification.md) |
| 修改输入、Replay、回滚或绘制 | [输入与确定性](playbooks/input-replay.md) |
| 调整房间、传输和多人集成 | [多人联机](playbooks/multiplayer.md) |
| 准备原版资源、字体、音乐、语言包 | [资源与语言](playbooks/resources.md) |
| 打包、交付和发布候选 | [发布](playbooks/release.md) |

维护文档记录现行实现与可重复操作。阶段交接、聊天记录、个人机器路径和一次性任务流水不进入文档；旧实现的背景可以从 Git 历史查阅。
