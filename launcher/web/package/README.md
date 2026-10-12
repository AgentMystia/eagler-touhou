# 浏览器游戏资源包

本目录拥有 `eagler-touhou/package/1` 描述、代次规划、pending/current 安装事务、IndexedDB 持久化、远程/本地安装及离线 ZIP 导入。它不是 npm 软件包的打包目录。

新安装使用完整 Package Store 模型。旧格式只能通过 `compat/` 的读取和迁移进入当前模型，不能新增旧格式生产者。邻接 `.d.mts` 是保留 JavaScript 模块的类型边界。

界面状态属于 `app/`；存储和安装策略应能独立于 React 测试。维护流程见[缓存与兼容](../docs/CACHE_AND_COMPATIBILITY.md)。
