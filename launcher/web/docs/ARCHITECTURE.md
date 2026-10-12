# Web 启动器架构

## 状态与职责

`app/BrowserLauncher.tsx` 是浏览器环境的组合入口，`app/launcher-bindings.ts` 定义组合所需的窄接口。`LauncherApplication.tsx` 接收模型、展示绑定和目录插槽；它不再接收一个同时包含产品、房间、触控、播放器和任意节点的可选字段集合。

| 层 | 主要入口 | 拥有的状态 |
| --- | --- | --- |
| 展示 | `app/components/` | 组件内部交互；通过输入和回调连接模型 |
| 应用模型 | `app/models/` | 设置、资源任务、Replay 列表、反馈和诊断视图 |
| 导航 | `app/navigation/` | 地址、表面状态、历史决策和离开确认 |
| 会话 | `app/session/browser-session.ts`、`room-session.ts` | 当前游戏/房间及一次启动的生命周期 |
| 临时文件会话 | `app/session/file-runtime.ts` | 文件 Runtime 的准备、取消、资源租约和过期请求 |
| Runtime 服务 | `app/services/runtime.ts`、`runtime/` | iframe 生命周期、请求、资源映射及运行错误 |
| 资源和存储 | `src/launcher/`、`app/services/` | 下载、安装事务、存档与导入导出 |
| 交付工具 | `lib/`、`host/`、`scripts/` | 站点输入、依赖安装、清单生成和产物验证 |

依赖通过实际调用进入组件。测试使用明确的模型/端口替身，不向生产入口增加任意 UI 注入和“全可选”默认分支。

## 导航与离开确认

`addresses.ts` 处理地址，`surface-state.ts` 描述可显示表面，`navigation-policy.ts` 计算离开决策；`NavigationDecision.tsx` 显示确认。`surface-navigation.tsx` 连接 React Router、浏览器历史和业务动作，保留单一导航协调者。

浏览器后退、关闭窗口、切换游戏和退出房间需要经过当前会话的退出规则。确认视图只返回选择，不能自行停止 Runtime 或重写资源状态。纯地址转换不读取房间或游戏存储。

导航协调文件仍较大；后续拆分应以独立订阅、状态拥有者和行为为依据，不能为了行数把同一个历史栈拆成多个互相修正的控制器。

## Runtime 生命周期与协议

`runtime/types.ts` 保存接口，`runtime/channel.ts` 负责消息通道，`runtime/resources.ts` 管理资源，`runtime/errors.ts` 定义可判断的错误。`runtime.ts` 负责组合和生命周期，避免多个模块同时创建、关闭或复用 iframe。

等待失败通过 `RuntimeWaitTimeoutError` 等错误类型判断；中文或英文显示文本不决定重试。启动请求使用当前代次和取消信号，过期回调不能取得新会话的页面、文件或输入所有权。

跨组件消息格式来自根 [contracts](../../../contracts/src/)。iframe 来源、消息类型、请求身份和当前会话必须同时匹配。资源清单、发布代次和 Runtime 就绪是不同边界：清单有效不代表首帧已产生。

## 资源与文件

资源准备决定当前游戏所需的 DATA、语言、音乐和 Runtime；Package 安装以完整代次提交。展示进度不拥有缓存提交权限。文件 Runtime 只持有当前操作所需的租约，取消、关闭和切换会话应释放租约，失败后不得覆盖已提交存档。

`browser-session.ts` 仍承担应用级协调；其内部的临时文件路径已经独立到 `file-runtime.ts`。继续拆分前先检查相应 mounted tests，尤其是取消、重新打开和旧请求迟到的行为。

## 输入、房间与渲染

键盘和触控服务把输入送入当前 Runtime；界面焦点、目录操作、聊天输入和游戏按键分别拥有输入。测试应检查实际按键是否被错误吞掉、松键是否送达，以及退出时是否清除状态。

房间模型通过独立服务协议处理成员、准备与连接。根 `services/netplay/` 拥有服务端房间和传输逻辑。Web 不实现游戏回滚规则；游戏与 common 负责固定步、输入确认、快照和重模拟。

React 组件负责普通页面呈现。早期启动及不兼容浏览器提示使用 `app/bootstrap/` 的明确 HTML/CSS/ES5 输入，不再从旧大 HTML 中用正则抽取脚本。正式样式是可编辑源码，不以整份 CSS 哈希作为验收。

## 相关流程

- [缓存与兼容](CACHE_AND_COMPATIBILITY.md)
- [Web 开发与验证](DEVELOPMENT.md)
- [跨游戏输入与 Replay](../../../docs/playbooks/input-replay.md)
- [多人联机](../../../docs/playbooks/multiplayer.md)
