# Web 启动器

[English](README.en.md) · [项目首页](../../README.md)

这里维护 eagler-touhou 的浏览器界面、资源安装与存储、Runtime 会话、静态站点组装和自托管工具。游戏逻辑属于根目录的游戏子模块，联机服务属于 `services/netplay/`。

## 开发

在项目根目录执行：

```text
npm ci --ignore-scripts
npm run dev
npm run build
```

源码环境要求以根 `package.json` 为准。`npm run dev` 提供当前 React 界面；可启动的游戏还需要 Runtime 和合法取得的资源。构建前端不会自动下载原版游戏数据。

## 文档入口

| 任务 | 文档 |
| --- | --- |
| 理解 UI、导航、会话和 Runtime 的职责 | [Web 架构](docs/ARCHITECTURE.md) |
| 修改代码、选择验证 | [Web 开发与验证](docs/DEVELOPMENT.md) |
| 维护缓存、离线更新和历史格式 | [缓存与兼容](docs/CACHE_AND_COMPATIBILITY.md) |
| 建立自己的站点 | [自托管指南](docs/SELF_HOSTING.md) |
| 查询配置和服务器要求 | [自托管参考](docs/SELF_HOSTING_REFERENCE.md) |
| 分开部署网页和资源 | [外部资源模式](docs/EXTERNAL_RESOURCE_MODE.md) |
| 理解资源来源及许可 | [资源说明](ASSETS.md)、[第三方软件](THIRD_PARTY.md) |

工程总体进度和跨游戏维护流程见[项目文档](../../docs/README.md)。站点 FAQ 和使用须知是用户帮助内容，不承担维护手册职责。
