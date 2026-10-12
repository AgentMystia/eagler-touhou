# Node 工程逻辑

本目录拥有 Web 构建、站点组装、Runtime Release、输入规范化、来源信息和产物验证等可复用 Node 逻辑。CLI 参数与一次性命令入口位于 `scripts/`；React 行为位于 `app/`，浏览器资源和存储位于 `src/launcher/`、`package/`。

`lib/contracts/` 是根 `contracts/src/` 的 Node 消费入口，不维护另一份数据定义。浏览器私有读取模型留在 Web 内部。

模块应有独立用途和明确输入。共享两处调用本身不足以成为公共架构层。详见 [Web 架构](../docs/ARCHITECTURE.md)。
