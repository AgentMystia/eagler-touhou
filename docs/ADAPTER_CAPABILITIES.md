# 适配能力分类

本页用于确定新增行为由谁实现、是否必须实现，以及怎样验收。它不是功能完成证明。游戏实现见[适配教程](ADAPTING_A_GAME.md)，消息字段的具体要求见 [Runtime 协议](GAME_ADAPTER_CONTRACT.md)与 `contracts/src/runtime-protocol.mts` 的语义表。

## 必须实现的 Runtime 行为

| 类别 | 要求 |
| --- | --- |
| 启动与生命周期 | ready/configure/launch/first-frame、错误与退出、旧会话隔离 |
| 输入 | 键盘、手柄、触控、释放和取消；同一逻辑动作保留作品语义 |
| 触控 | 多指独立、菜单/对话切换、灵敏度、限速/不限速移动及 Replay 记录；见[行为约束](ADAPTER_BEHAVIOR_INVARIANTS.md) |
| 呈现 | 固定逻辑与显示刷新分开，保持实际状态消费顺序、附着视觉和亚像素位置 |
| 音频 | 正常 OGG 和 none、原曲切换/循环/淡出、SFX、前后台恢复；stream/full 是解码策略 |
| 用户文件 | 保存根目录白名单、list/read/write/remove/sync、重载持久化 |
| Replay | 原版可表达输入使用原版格式；扩展输入有版本和校验，能导入、播放、导出及重载 |

“必须实现”不意味着改写每作的原生规则。角色速度、菜单时序、碰撞、Deathbomb、关卡和 RNG 保留目标作品的语义；跨作统一只适用于已明确的产品行为。限帧配置不应改变游戏时间。

## 由 Launcher 提供的行为

目录、设置持久化、资源安装/更新/导入、语言选择、下载进度、存档/Replay 管理界面、App Shell 与离线资源管理由现有 Web 工程提供。游戏实现对应协议和文件行为，不再复制一套界面或数据库。接入步骤见 [Web 新游戏接入](../launcher/web/docs/ADAPTING_A_GAME.md)。

放大镜、触控布局编辑器和按钮是否显示属于 Launcher；游戏实际需要的值才进入 `RuntimeConfigureOptions`。`RUNTIME_CONFIGURE_OPTION_BEHAVIOR` 区分 required、profile-required、optional 和 diagnostic。

## 可选功能与启用后的义务

| 功能 | 静态声明 | 启用后需要验证 |
| --- | --- | --- |
| 多人 | `multiplayerRuntime`、`multiplayer` | 产品支持的席位、难度/机体、输入确认、结束/重开、断线恢复、Replay；声明旁观时还要真实旁观数据流 |
| 练习 | `features.thprac` | 真实实现的练习入口、输入、语言、存档与场景钩子 |
| 语言包 | `features.languages` | 语言资源和字体、原版日文隔离、缺包回退与持久化偏好 |
| MIDI | `musicCapabilities.midi` | 时序、循环、淡出、无远端 MIDI 时的本地行为 |
| 聚焦判定点增强 | `features.focusHitbox` | 对应显示与作品自身判定点的语义 |

可选功能的“未启用”与“启用后少实现了一部分”不同。Runtime Release 和站点可减少可选功能，但不能超过产品能力上限。合作游戏的救援、幽灵、资源转移规则不适用于 TH09 的对战模式。

## 实现细节、格式适配与兼容

- SDL/miniaudio 后端、触控轨迹工具、快照存储和音频队列是实现细节，不是新的用户功能。
- 原版 DATA、score、Replay、音乐和语言包的不同格式由专门的格式处理器拥有。不要为某个格式增加第二套通用启动流程。
- 历史协议别名、旧清单和迁移路径只用于读取已存在的数据。新游戏使用当前字段，不继续输出旧格式。
- 诊断 WASM、Replay Verifier 和 Presentation Lab 服务维护者，不给正式 Runtime 协议增加诊断命令。

## 判断和验收

新增行为前先确定：它是所有 Runtime 的共同要求、已启用功能的一部分、真正的产品差异、格式差异，还是历史读取兼容。只有真实产品差异进入 Product Catalog；只在一个 UI 中使用的状态留在 Web。

验收记录具体 Runtime、输入、资源、浏览器/设备和场景。协议模型、共享算法、源码编译与实际游戏各自提供不同证据，方法见[验证](playbooks/verification.md)。已登记的能力和已通过的行为应分别记录。
