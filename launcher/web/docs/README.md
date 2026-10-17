# Web 启动器文档

[项目文档](../../../docs/README.md)负责游戏依赖、通用构建和工程进度。本目录负责浏览器启动器及其站点交付。

- [架构](ARCHITECTURE.md)：状态归属、导航、Runtime 和服务边界。
- [开发与验证](DEVELOPMENT.md)：修改入口、测试选择及生成内容。
- [接入新游戏](ADAPTING_A_GAME.md)：产品登记、资源准备、Runtime Release 和实际前端流程。
- [产品呈现](PRODUCT_SURFACE.md)：界面能力和公开产品信息。
- [存储测试](RUNTIME_STORAGE_TESTING.md)：真实 Runtime 的用户文件与重载验证。
- [缓存与兼容](CACHE_AND_COMPATIBILITY.md)：安装事务、离线更新和历史数据。
- [Runtime、Launcher 与 Package](RUNTIME_LAUNCHER_PACKAGE.md)：资源、会话和持久化的拥有关系。
- [PWA 与更新](PWA.md)：安装、Shell/Runtime 分工和离线验证。
- [房间连接诊断](ROOM_NETWORK_UI.md)：开赛前双端 RTT、直接/TURN/中继探测。
- [触控功能键](TOUCH_FUNCTION_KEY.md)：共享 C 键输入与作品自身操作。
- 语言资源：[TH09](TH09_LOCALIZATION.md)、[TH11](TH11_LOCALIZATION.md)、[TH15](TH15_LOCALIZATION.md)；[自动对话编译](THCRAP_AUTO_DIALOGUE.md)。
- [自托管指南](SELF_HOSTING.md)：从输入到可验证站点。
- [自托管参考](SELF_HOSTING_REFERENCE.md)：配置、HTTP、联机和迁移要求。
- [外部资源模式](EXTERNAL_RESOURCE_MODE.md)：网页站点与资源站点的成对交付。
- [用户常见问题](FAQ.md)：会构建到站点的帮助内容。

游戏源码的移植、运行协议和专题教程在[根目录游戏文档](../../../docs/README.md)。本目录负责 Launcher 如何消费那些接口。
