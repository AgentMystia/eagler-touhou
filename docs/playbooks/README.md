# 游戏适配与工程教程

先选实际改动所属的教程，再从其中的代码入口展开。游戏规则以目标作品源码为准；教程中的另一作经验用于实现参考，不自动成为本作规则或验收结果。

## 开始适配

- [游戏 Runtime 适配教程](../ADAPTING_A_GAME.md)：从源码、平台层到 Runtime 和真实运行。
- [Runtime 接入协议](../GAME_ADAPTER_CONTRACT.md)：消息、生命周期、输入、文件与资源边界。
- [能力分类](../ADAPTER_CAPABILITIES.md)和[行为约束](../ADAPTER_BEHAVIOR_INVARIANTS.md)：必做项、可选功能和具体交互语义。
- [Web 新游戏接入](../../launcher/web/docs/ADAPTING_A_GAME.md)：目录、资源、Package、站点与前端流程。
- [源码与工作区](adaptation-worktrees.md)：明确源码、依赖、变体和合并来源。

## 游戏实现

| 问题 | 教程 |
| --- | --- |
| 浏览器、shell、SDL 与 WebGL | [Web Runtime](web-runtime.md) |
| 键盘、手柄、触控和手势 | [输入与触控](input-touch.md) |
| BGM、SFX、解码和恢复 | [音频](audio.md) |
| 额外绘制、插值与状态消费 | [高刷新率](interpolation.md) |
| 绘制、数据上传和性能定位 | [渲染与性能](performance-rendering.md) |
| Replay、固定步和原版对照 | [Replay 与确定性](replay-determinism.md) |
| 语言、字体和 thcrap | [语言适配](localization-thcrap.md) |
| 练习模式与共享工具 | [thprac](thprac.md) |
| 移动浏览器、视口和恢复 | [移动浏览器](mobile-browser.md) |
| 多人集成、房间和会话 | [多人](multiplayer.md) |
| 启动时序与校准 | [Adonis 适配](adonis-adaptation.md) |
| 快照、输入、补帧与优化 | [回滚](rollback.md) |

## 构建、验证和交付

| 工作 | 教程 |
| --- | --- |
| 源码构建和 Runtime Release | [Runtime 构建](runtime-build.md) |
| 选择验证与添加回归 | [验证](verification.md)、[验收方法](testing-acceptance.md) |
| 原版资源、字体与准备工具 | [资源](resources.md) |
| 自托管包与发布候选 | [发布](release.md) |
| 反向代理、中继与迁移 | [部署与网络](deployment-network.md) |

游戏代码路径相对其子模块，Web 路径在 `launcher/web/`，共享运行支持在 `eagler-common/`。旧调查案例保留固定源码和环境，只用于理解历史问题；本机任务日志和临时部署事实留在独立证据目录。
