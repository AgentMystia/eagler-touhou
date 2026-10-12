# Web 开发与验证

## 入口

所有命令先从项目根目录执行；`--workspace @eagler/web` 选择 Web 工程。独立自托管包则在包根目录运行自己的 npm 命令。

```text
npm run dev
npm run build
npm run typecheck --workspace @eagler/web
npm run typecheck:tools --workspace @eagler/web
npm run test:ui:unit --workspace @eagler/web
npm run test:ui:artifact --workspace @eagler/web
```

`npm run check` 是工程组合检查，耗时长。`launcher/web/scripts/check.mjs` 检查受 Git 管理的源码及声明的核心测试，不把开发者未跟踪的实验文件当成发布输入。新增正式测试后，要检查 `tests/` 中的测试计划是否已纳入它。

## 按改动选择证据

| 改动 | 最小有意义的验证 |
| --- | --- |
| 模型、导航、会话 | 对应 `tests/ui/` 的行为测试；涉及 DOM 生命周期时运行 mounted tests |
| 类型和依赖边界 | UI/tools 类型检查及受影响的构建 |
| 样式、焦点、触控 | 当前组件的真实浏览器交互和必要布局检查 |
| Runtime 启动、存档 | 真实 Runtime、真实资源和存储重载；标明具体作品 |
| Package、离线更新 | 安装/更新事务测试及 PWA 浏览器用例 |
| 站点清单和打包 | 产物测试、`verify:server`，必要时隔离解压后的冷启动 |
| 房间服务 | 根 `npm run test:services`；改发布包时补独立服务包的实际连接 |

浏览器测试入口集中在 `package.json`，不默认把所有浏览器脚本塞进每次检查。需要原版 DATA、字体、诊断 Runtime 或多个真实浏览器的测试必须提供输入，并记录实际运行范围。

## 测试边界

UI 单元组与产物组分别由 `tests/run-ui.mjs` 调度。产物组运行真实编译，串行执行，避免并行重建同一输出。`tests/browser/support/` 的 fixture 使用当前源码；合成 Runtime 可验证协议、离线和生命周期，不能替代游戏内容验收。

使用稳定行为断言：地址变化、焦点、事件、持久化结果、代次切换和错误类型。只有协议魔数、文件格式、清单身份等真实边界才固定值。不要冻结整份 CSS、源文件字节、历史提交文本、函数出现次数或“恰好若干行”的实现细节。

## 用户帮助内容

- `docs/FAQ.md`：常见问题。
- `content/FIRST_USE_NOTICE.md`：首次使用须知。
- `content/MULTIPLAYER.md`：多人玩法说明。

修改后运行：

```text
npm run build:content --workspace @eagler/web
```

`public/content/` 是生成 HTML。不要直接编辑 HTML 再让 Markdown 追随。帮助内容写当前用户能执行的操作，不包含开发聊天、个人机器路径、未确定计划或未经验证的兼容性承诺。

## 构建缓存与依赖

根 npm lock 是源码依赖入口；独立自托管包生成自己的 lock。contracts 构建使用按输入身份命名的不可变代次。浏览器服务构建仍使用目录锁和可变输出，维护限制见[缓存与兼容](CACHE_AND_COMPATIBILITY.md)。

发布流程见[项目发布 playbook](../../../docs/playbooks/release.md)。本地构建与检查不会授权上传或部署。

早期兼容提示的 bootstrap JavaScript 必须保持 ES5。根 Prettier 配置对该目录关闭尾逗号；格式化后仍由 ES5 解析与行为测试检查，不能为通过格式检查引入旧浏览器无法解析的语法。
