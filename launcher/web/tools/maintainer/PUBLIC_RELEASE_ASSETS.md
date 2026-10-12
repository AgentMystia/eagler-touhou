# 公共交付资产

正式候选可能包含原版 DATA、Hosted 资源和离线游戏包，不能整体作为公开源码附件。公共资产从同一已验证候选中派生：

| 资产 | 内容 |
| --- | --- |
| `EaglerTouhou-SelfHost.zip` | Host 工具、编译后的 Web、可再分发共享资源、Runtime Release，以及所选游戏的空输入目录 |
| `EaglerTouhou-Runtime.zip` | 同一 Runtime Release 的传输 ZIP，不含原版 DATA |
| `SHA256SUMS.txt`、`release-assets.json` | 完整性与来源信息 |

从项目根目录执行：

```text
npm run package:release-assets -- --candidate=CANDIDATE_DIRECTORY --output=NEW_DIRECTORY --unicode-font=FONT_FILE --font-license=LICENSE_FILE
```

`scripts/build-release-assets.mjs` 验证候选并调用跨平台自托管/ZIP 构建。输出不可与已存在的交付目录混用。字体必须具有此次分发需要的许可。

Hosted、game-package、offline-zip、原版目录、用户文件、密钥和私有日志不属于以上公开资产。

获得部署授权后，从全新浏览器配置对实际发布游戏逐作执行 `verify-public-first-install.mjs URL --game=thXX`，再验证存档和所发布的游戏能力。静态验证不代替浏览器首次安装和游戏验收；这些工具不上传 GitHub 或切换生产。
