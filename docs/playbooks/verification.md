# 验证与回归

## 按问题选择证据

| 变更 | 必要验证 | 不足以证明的内容 |
| --- | --- | --- |
| 协议、路径、归档、清单 | 正常输入、损坏输入、边界、拒绝后的状态；真实文件闭包 | 源文件里含有某个函数名 |
| 设置、导航、会话取消 | 挂载当前 React 组件，覆盖取消、后退、旧异步回调和卸载 | 对照旧提交的 JSX 或中文句子 |
| 游戏构建 | 实际工具链编译及产物校验 | 全关卡、声音、存档与联网正确 |
| 游戏逻辑、输入、回滚 | 真实生产对象的 C++ 测试和游戏/Replay 验证 | 手写另一套游戏规则后验证自己 |
| UI 布局与浏览器行为 | 当前组件、实际 CSS、浏览器视口和输入 | jsdom 的几何结果或整份 CSS 哈希 |
| 自托管/服务交付 | 解压到独立目录，安装包自身依赖，再启动或组装 | 在开发仓库旁边运行成功 |

## 还原兼容对照

游戏还原代码先比反编译来源，再判断移植差异。类型擦除、临时释放链或 draw 写状态本身不是 bug 证据。查证须固定来源提交、函数及调用点，区分原行为、平台适配与新增 Web 路径；本节只记录已核对的源码事实，不代表完整玩法、目标 ABI 或长 Replay 已验收。

| 固定来源 | 已核对事实 |
| --- | --- |
| GensokyoClub/th06 `master`，`cc475a0bc3fef38683b0f02224c87ddba0a021d9` | [Chain.hpp:24](https://github.com/GensokyoClub/th06/blob/cc475a0bc3fef38683b0f02224c87ddba0a021d9/src/Chain.hpp#L24) 定义 `void*` 参数回调；[Chain.cpp:246](https://github.com/GensokyoClub/th06/blob/cc475a0bc3fef38683b0f02224c87ddba0a021d9/src/Chain.cpp#L246) 的 `ReleaseSingleChain` 构建临时链后执行 Cut 并释放临时节点；[Supervisor.cpp:502](https://github.com/GensokyoClub/th06/blob/cc475a0bc3fef38683b0f02224c87ddba0a021d9/src/Supervisor.cpp#L502) 的 `DrawFpsCounter` 更新帧计数、时间和统计字段 |
| some100/th07 `main`，`9725615ecfa4858a8714e7267910efdee0c287ac` | [Chain.hpp:19](https://github.com/some100/th07/blob/9725615ecfa4858a8714e7267910efdee0c287ac/src/th07/Chain.hpp#L19) 定义 `void*` 参数回调；[Chain.cpp:215](https://github.com/some100/th07/blob/9725615ecfa4858a8714e7267910efdee0c287ac/src/th07/Chain.cpp#L215) 的 `ReleaseSingleChain` 同样使用临时链；[Stage.cpp:554](https://github.com/some100/th07/blob/9725615ecfa4858a8714e7267910efdee0c287ac/src/th07/Stage.cpp#L554) 消费并清除 `clearBackground`，[Stage.cpp:685](https://github.com/some100/th07/blob/9725615ecfa4858a8714e7267910efdee0c287ac/src/th07/Stage.cpp#L685) 消费并清除 `isDarkening` |

核对方法：只读确认对照仓库 HEAD 与上述提交一致，读取函数体和注册/调用位置，再逐项比较当前作品实现。回调需检查实际签名、调用约定、参数及目标 ABI；释放链需检查 Cut 回调和遍历顺序；绘制需检查写入字段的读取者、原 draw 次数及额外 draw 的消费时机。确认实际差异后，用真实对象、目标运行或原版 Replay 复现，再决定是否修复。不能仅为 typed callback 或“纯绘制”目标更改原行为。

## 入口

以下命令在根目录执行：

```text
node launcher/web/scripts/check.mjs
npm run typecheck --workspace @eagler/web
npm run typecheck:tools --workspace @eagler/web
npm run test:ui:unit --workspace @eagler/web
npm run test:ui:artifact --workspace @eagler/web
npm run test:services
node --test tools/tests/runtime-package.test.mjs
```

原生 CTest：

```text
cmake -S eagler-common -B .cache/build/common -DEAGLER_COMMON_BUILD_TESTS=ON
cmake --build .cache/build/common --config Release --parallel 4
ctest --test-dir .cache/build/common -C Release --output-on-failure
cmake -S th06/tests -B .cache/build/th06-tests
cmake --build .cache/build/th06-tests --config Release --parallel 4
ctest --test-dir .cache/build/th06-tests -C Release --output-on-failure
```

TH07 对应替换 `th06` 为 `th07`。根 `npm run check:workspace` 组合这些检查。Windows 可按本机环境选择 Ninja、MSVC 或其他实际可用的 C++ 工具链；记录运行时使用的编译器。

真实 Chromium 检查在 `launcher/web` 执行，例如：

```text
node tests/browser/test-content-fragments.mjs
node tests/test-multiplayer-quick-chat-browser.mjs
python tests/browser/test-pwa-boot.py --browser=chromium
```

浏览器路径可通过相关入口的明确选项或 `EAGLER_CHROMIUM_EXECUTABLE` 指定。需要真实游戏的测试应先准备站点和资源，不能把合成 Runtime 的结果填成游戏通过。

`npm run test:runtime-document:browser --workspace @eagler/web` 验证 iframe 当前文档观察，覆盖导航、about:blank 与跨源拒绝，不替代真实游戏启动。浏览器测试可用 `EAGLER_LAUNCHER_TEST_ROOT` 指定已构建 Launcher 目录，缺件不得回退旧页面。准备任务取消回归、变体 collection/Host、client-services 代次以及 Python 环境身份分别使用对应行为测试；新 Python 锁的冷安装、三系统与全 UI 整组结果单独记录，当前范围见[审计证据](../ENGINEERING_AUDIT.md#本轮实际验证)。

## 保留和删除什么测试

保留协议版本、持久化格式、公开错误类别、受管路径、资源摘要和旧数据读取等真正边界的检查。资源哈希是完整性校验，不是源码快照。

不保留以旧提交内容、函数源码切片、固定分支数、整份样式哈希或编译对象字节相同为目标的常规门禁。历史对照可以辅助一次调查；长期回归应落到真实行为和最小失败场景。不要为格式化、文档改名等低风险整理机械添加测试。

失败报告需要包含入口、版本、输入范围、失败阶段和实际输出。短 trace、未完成 Replay、未覆盖状态组须如实标记，不替换基准文件来消除差异。当前验证快照见[审计证据](../ENGINEERING_AUDIT.md#本轮实际验证)。[作品进度](../TOUHOU_PROGRESS.md)保留项目原登记，不由此推导本轮未执行项目已通过。
