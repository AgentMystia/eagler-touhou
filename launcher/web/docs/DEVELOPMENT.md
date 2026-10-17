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

`npm run check` 组合核心、类型和 UI 检查；服务测试单独运行 `npm run test:services`。`launcher/web/scripts/check.mjs` 的源码扫描读取 Git 管理的文件，核心测试由测试计划选择；UI 调度器读取 `tests/ui/` 的实际测试文件。新增测试时核对相应调度入口，发布源码是否跟踪与本地测试是否执行是两个检查。

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

浏览器测试入口集中在 `package.json`，不默认把所有浏览器脚本塞进每次检查。`EAGLER_LAUNCHER_TEST_ROOT` 可指定已构建 Launcher，JS/Python helper 使用目标目录，缺件不回退旧页面。`test:runtime-document:browser` 验证当前 iframe 文档 URL、导航及跨源拒绝，不能用 iframe 的初始 src 代替当前文档。旧 `verify:unified` 已退役。需要原版 DATA、字体、诊断 Runtime 或多个真实浏览器的测试必须提供输入，并记录实际运行范围。

## 测试边界

UI 单元组与产物组分别由 `tests/run-ui.mjs` 调度。产物组运行真实编译，串行执行，避免并行重建同一输出。`tests/support/` 与 `tests/ui/` 的 fixture 使用当前源码；合成 Runtime 可验证协议、离线和生命周期，不能替代游戏内容验收。

使用稳定行为断言：地址变化、焦点、事件、持久化结果、代次切换和错误类型。只有协议魔数、文件格式、清单身份等真实边界才固定值。不要冻结整份 CSS、源文件字节、历史提交文本、函数出现次数或“恰好若干行”的实现细节。

准备任务回归在 `tests/ui/preparation-activity.test.mjs`，覆盖过期 runtime plan、旧清理、独立取消与外部 abort。启动 fixture 的 `startupException`、`runtimeSnapshot`、`warnings` 用于保留失败现场，无法复现的失败不推断根因。定向组通过与全 UI 整组结果分别记录，当前证据见[工程审计](../../../docs/ENGINEERING_AUDIT.md#本轮实际验证)。

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

根 npm lock 是源码依赖入口；独立自托管包生成自己的 lock。contracts 和 client-services 构建使用按输入身份命名的不可变代次。client-services 在独立暂存目录编译并复核源码、依赖闭包和输出摘要，发布后调用者持有固定句柄；并发构建不共享目录锁，损坏代次拒绝复用，不能覆盖已发布代次。Python 精确版本及 wheel hash 锁的维护入口见[项目开发指南](../../../docs/DEVELOPMENT.md#python-依赖锁)。

发布流程见[项目发布 playbook](../../../docs/playbooks/release.md)。本地构建与检查不会授权上传或部署。

早期兼容提示的 bootstrap JavaScript 必须保持 ES5。根 Prettier 配置对该目录关闭尾逗号；格式化后仍由 ES5 解析与行为测试检查，不能为通过格式检查引入旧浏览器无法解析的语法。
