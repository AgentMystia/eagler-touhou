# eagler-touhou 自托管包

[English](README.en.md)

本包包含预编译启动器、Host 工具和 Runtime，不包含原版游戏 DATA。先阅读[自托管指南](docs/SELF_HOSTING.md)，按 Runtime Release 的游戏集合准备自己的资源、字体和可选配置。

```text
npm run host:doctor
npm run host:build
npm run verify:server -- dist/site
```

[配置参考](docs/SELF_HOSTING_REFERENCE.md) · [外部资源模式](docs/EXTERNAL_RESOURCE_MODE.md) · [第三方说明](THIRD_PARTY.md)

构建不会自动上传或部署。请备份自己的资源、配置、存档和上一站点版本。
