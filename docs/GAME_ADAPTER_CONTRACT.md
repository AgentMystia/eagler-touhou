# 游戏 Runtime 接入协议

本页定义游戏 Runtime 与 Launcher 的消息和资源边界。类型与命令语义以 [contracts/src/runtime-protocol.mts](../contracts/src/runtime-protocol.mts) 为准，产品身份以 [product-catalog.mts](../contracts/src/product-catalog.mts) 为准。游戏实现步骤见[适配教程](ADAPTING_A_GAME.md)，前端消费步骤见 [Web 接入](../launcher/web/docs/ADAPTING_A_GAME.md)。

## 消息封装与会话身份

协议名为 `eagler-touhou/1`。命令、事件和回复都包含：

| 字段 | 含义 |
| --- | --- |
| `protocol` | 固定协议名 |
| `game` | 产品登记中的作品 ID |
| `epoch` | 此次 Runtime 导航的正整数 `runtimeEpoch` |
| `command` / `event` | 命令或事件名 |
| `request` | 需要回复的请求 ID；回复原样携带 |
| `ok` | 回复是否成功，失败时附带 `error`，必要时附带 `errno` |

消息的参数直接放在消息对象上，没有另外一层 `payload`。示例使用已经登记的 TH15：

```json
{
  "protocol": "eagler-touhou/1",
  "game": "th15",
  "epoch": 1,
  "command": "configure",
  "request": "configure-1",
  "music": "none",
  "language": "ja",
  "options": { "touchEnabled": true, "touchSensitivity": 100 }
}
```

成功回复：

```json
{ "protocol": "eagler-touhou/1", "game": "th15", "epoch": 1, "request": "configure-1", "ok": true }
```

Runtime 检查 `event.source === parent`、同源、协议、作品和 epoch。Launcher 对照当前 iframe 文档和会话做同样检查。替换 iframe 文档后，即使 `contentWindow` 对象相同，也不能接受旧 epoch。直接 DATA 桥接和即时输入通道同样绑定会话身份。

## 启动和结束

`ready → configure → resources（如需要）→ launch → first-frame` 是正常启动路径。配置和启动各自回复请求。

- `ready`：消息、存储和 DATA 桥接已能接受命令。
- `first-frame`：启动后的游戏确实完成一次呈现。
- `runtime-info`：成功启动后的渲染器、架构和版本信息。
- `frame-health` / `audio-health`：运行观察，不能驱动游戏逻辑。
- `transfer`：实际资源操作的进度。
- `exit`：当前游戏正常结束；持久化保存应在结束路径完成。
- `error`：当前会话的失败信息。

不要用定时器代替首帧，也不要在运行中无条件重做 `configure`。启动失败、超时或会话被替换时，未完成请求必须失败并释放资源，不能迟到后改动新会话。

## 命令和数据

| 命令 | 参数与行为 | 回复 |
| --- | --- | --- |
| `configure` | `music`、`language`、资源集合及 `options`；验证后应用，拒绝不安全的运行中重配置 | 必须 |
| `resources` | `resources`；安装追加的已声明资源 | 必须 |
| `launch` | 启动已配置的游戏 | 必须 |
| `keyboard` | `code`、`down`；一个键盘状态变化 | 可省略 |
| `keyboard-clear` | 清除按住的键盘状态 | 可省略 |
| `touch-controls` | `fireEnabled`、`focusEnabled`、`bombSerial`、`escapeSerial`、`joystickX/Y`、`touchSensitivity` | 可省略 |
| `direct-touch` | `type: down/move/up`、`id`、游戏表面归一化 `x/y` | 可省略 |
| `touch-cancel` | 释放触点与手势所有权 | 可省略 |
| `list` | 枚举允许的用户文件 | 必须 |
| `read` | `path`；读取允许的文件 | 必须 |
| `write` | `path`、字节数组 `bytes`；校验并保存 | 必须 |
| `remove` | `path`；删除允许的文件并持久化 | 必须 |
| `sync` | 刷新游戏保存状态和持久化文件系统 | 必须 |

可省略 ACK 的输入仍是必须实现的行为。`retry-music` 只适用于 Runtime 自己拥有的可重试音乐传输；`thprac-mouse` 随练习功能启用；`network-cancel` 适用于对应的原生联机入口。条件不满足时不能伪造成功回复。详细阶段和条件见 `RUNTIME_PROTOCOL_COMMAND_BEHAVIOR`。

## 配置归属

`RuntimeConfigureOptions` 和 `RUNTIME_CONFIGURE_OPTION_BEHAVIOR` 描述每个字段。触控、显示节奏、音乐解码等是游戏实际消费的配置；多人、练习、语言相关字段按对应功能启用。

Launcher 的放大镜、触控布局编辑器、重启按钮是否显示等界面状态不进入 Runtime。持久化偏好的名字也不等于协议字段，例如限帧传入 `limitPresentationTo60`。新适配不复制 `unlimitedTouch`、`th06FocusHitbox`、`thpracSession` 等历史别名；兼容项单独列在协议源码中。

## 文件和 DATA

Runtime 只开放产品保存根目录下的 score、config 和 Replay 白名单，拒绝 `..`、越界绝对路径以及不属于本作的文件。普通/多人根目录分别隔离。写入和删除成功必须有持久化结果；只读操作不能修改用户文件。

DATA、音乐和语言资源由父页面按 Package 与资源代次准备；游戏按声明的 provider 和布局消费。Runtime 不决定 Package 安装是否提交，不自行更换资源身份。`emscripten-preload` 与 `retail-memory` 的具体组合见[适配教程](ADAPTING_A_GAME.md#3-选择-runtime-和-data-形态)。

## 产品、构建和站点能力

| 层 | 拥有的事实 |
| --- | --- |
| Product Catalog | 作品、默认入口、保存规则、资源形态、可选能力上限 |
| Runtime Release | 实际可分发的普通/多人变体、文件集合、DATA 布局与构建能力 |
| Site Manifest | 这次部署的 Runtime 地址、资源身份、语言和音乐可用性 |
| Package Descriptor / Store | 可安装文件、挂载目标、资源组件和已提交代次 |
| Launcher | 根据这些声明选择、准备并启动当前会话 |

站点可以少提供某项可选能力，不能超过产品和 Runtime 的能力。当前 Release 支持普通-only、多人-only 和混合集合；同作两个变体必须使用相同 DATA 布局。详细产品字段的接入操作在 [Web 文档](../launcher/web/docs/ADAPTING_A_GAME.md)，无需在游戏 shell 复制一份目录或发布清单。

## 浏览器生命周期和验证

失焦时释放输入；进入后台时按作品调度规则暂停并保存；恢复时从浏览器允许的手势恢复 AudioContext。Web 失焦与页面隐藏是两个事件，具体行为见[行为约束](ADAPTER_BEHAVIOR_INVARIANTS.md)。

至少验证一次正常启动/首帧、坏消息和坏路径拒绝、过期 epoch、取消后迟到回复、文件持久化与重载。协议模型用例在 `launcher/web/tests/test-runtime-protocol-model.mjs`，当前会话和文件事务用例在 Web tests；真实 Runtime 还需自己的浏览器和存储验收。字符串中出现命令名不能替代行为验证。
