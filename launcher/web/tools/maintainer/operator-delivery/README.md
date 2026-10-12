# 运营交付模板

此目录保存公开模板和示例 profile，不包含实际主机配置或资源。构建要求显式传入与当前候选匹配的 profile，不再默认采用某次历史交付的游戏、音乐、迁移或服务组合。

在 `launcher/web/` 执行：

```text
node tools/maintainer/build-operator-delivery.mjs --release=FORMAL_RELEASE --output=NEW_DIRECTORY --profile=PROFILE_FILE --archive=NEW_ZIP
```

`profile.example.json` 仅演示字段，必须按实际站点能力修改。工具先验证候选及 Hosted，再检查 profile，最后生成交付与校验信息。目录和 ZIP 均不得已存在；`--archive` 可省略。

生成包可能含原版衍生资源，只交付给有权接收的运营者，保存在源码外。静态验证不会自动赋予浏览器或完整游戏验收等级。
