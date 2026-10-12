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

## 保留和删除什么测试

保留协议版本、持久化格式、公开错误类别、受管路径、资源摘要和旧数据读取等真正边界的检查。资源哈希是完整性校验，不是源码快照。

不保留以旧提交内容、函数源码切片、固定分支数、整份样式哈希或编译对象字节相同为目标的常规门禁。历史对照可以辅助一次调查；长期回归应落到真实行为和最小失败场景。不要为格式化、文档改名等低风险整理机械添加测试。

失败报告需要包含入口、版本、输入范围、失败阶段和实际输出。短 trace、未完成 Replay、未覆盖状态组须如实标记，不替换基准文件来消除差异。当前验证快照见[审计证据](../ENGINEERING_AUDIT.md#验证证据)。
