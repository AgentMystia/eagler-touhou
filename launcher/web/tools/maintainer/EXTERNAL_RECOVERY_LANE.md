# 外部资源站点恢复

仅当已发布的 Hosted Package 代次仍是实际资源源站，需要重建配套测试/恢复网页时使用本入口。正式候选由项目 `npm run release` 生成。

在 `launcher/web/` 执行：

```text
node tools/maintainer/recover-external-site.mjs --source=HOSTED_DIRECTORY --runtime-release=RUNTIME_DIRECTORY --output=NEW_DIRECTORY --profile=web-validation-external-recovery --netplay-relay=wss://play.example.com/eagler-netplay/ --origin-migration=none
```

输出必须是新目录，只接受 `web-validation-*` profile。源站清单、DATA 布局及 Runtime 必须通过验证。允许显式覆盖 Relay、Origin 迁移等部署元数据；DATA、音乐、语言、Package 指针及描述身份必须沿用源代次。

输出记录源 Release、Runtime 清单身份、游戏和覆盖项。不要为改变 Relay 地址重新构建一个内容能力不同的 Hosted 源，也不要把恢复产物直接标为正式 Release。

切换流程见[外部资源模式](../../docs/EXTERNAL_RESOURCE_MODE.md)。
