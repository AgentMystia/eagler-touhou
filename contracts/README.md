# 共享数据与协议

[English](README.en.md)

`@eagler/contracts` 是主项目内部 workspace，保存 Web、Node 工具和独立服务共同使用的 TypeScript 数据边界：产品登记、Runtime 消息、站点/发布清单、资源模式与代次、联机时序及快捷消息。

`src/compat/` 只读取有实际持久化或已发布格式依据的历史数据。浏览器私有视图模型、导航和存储事务留在 Web；C++ 模拟算法属于 eagler-common。新增接口应指出实际消费者，不建立无人使用的“未来平台通用接口”。

源码导出由 `package.json` 指向 `src/*.mts`。Web 工具的 `lib/contracts-build.mjs` 生成按输入身份命名的不可变 JS 代次，服务交付由 esbuild 打包其实际闭包。包当前不作为公共 npm 产品发布。

从项目根运行 `npm ci --ignore-scripts`、`npm run check`。更改协议需覆盖有效/无效输入、代次和旧格式读取，不用接口自报能力字典替代真实行为。

[工程架构](../docs/ARCHITECTURE.md) · [验证流程](../docs/playbooks/verification.md)

[Runtime 协议](../docs/GAME_ADAPTER_CONTRACT.md)说明游戏 shell 如何消费这些数据；[Web 新游戏接入](../launcher/web/docs/ADAPTING_A_GAME.md)说明前端与资源登记。
