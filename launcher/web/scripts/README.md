# 构建和验证命令

本目录包含跨平台 CLI：`build-*`/`prepare-*` 处理构建输入，`package-*`/`release.mjs` 生成交付物，`verify-*`/`check.mjs` 验证可观察行为和产物，`serve*` 用于本地服务。

可复用策略放到 `lib/`，测试放到 `tests/`。CLI 解析和错误说明保持靠近入口，不能把业务规则藏在 shell 包装器中。通用命令不依赖 PowerShell。

日常使用 [Web 开发指南](../docs/DEVELOPMENT.md)中的 npm 入口。基础设施探测及运营交付工具见 [维护工具](../tools/maintainer/README.md)，正式候选见[项目发布流程](../../../docs/playbooks/release.md)。
