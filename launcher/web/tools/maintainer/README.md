# 维护与运营工具

本目录保存可公开的探测工具和脱敏模板。原版资源、私有配置、密钥、日志、交付 ZIP 和验收原始记录不放入源码。

| 入口 | 用途 |
| --- | --- |
| `verify-public-relay-fallback.mjs`、`verify-public-targeted-relay.mjs` | 验证指定 WSS 的实际中继链路 |
| `verify-public-turn-quality.py` | 测量显式指定的 TURN 目标 |
| `verify-public-first-install.mjs` | 从全新 Chromium 配置安装 Package，等待真实 Runtime 首帧 |
| `verify-thcrap-real.mjs` | 验证实际 thcrap 准备链路 |
| `recover-external-site.mjs` | 从已有 Hosted 代次恢复 external 验证产物 |
| `build-operator-delivery.mjs` | 按显式能力 profile 生成运营交付包 |

公共 Release 资产现在使用 `scripts/build-release-assets.mjs`，见[公共交付资产](PUBLIC_RELEASE_ASSETS.md)。通用产物构建不再经过 PowerShell。

探测结果只能说明实际目标、时间、版本和条件。首次安装的超时是防挂死限制，耗时不是性能通过线。构建、探测和模板不授权上传或生产切换。
