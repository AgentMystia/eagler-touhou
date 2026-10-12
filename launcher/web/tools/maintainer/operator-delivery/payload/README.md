# eagler-touhou 运营交付包

本包来自一个已验证的正式 Release。部署 `site/`，具体游戏、语言、音乐、练习、Runtime 和服务要求以 `operator-profile.json`、`operator-delivery.json` 及站点清单为准。

`verification-report.json` 记录继承的验证范围。文件结构通过不等于目标浏览器和游戏验收通过。`operator-checksums.txt` 用于检查交付完整性；在源工程可调用运营交付验证器复核。

保留交付原件，在部署副本上配置。阅读 `services/README.md` 后确定静态服务器、Relay 和迁移阶段；没有实际迁移需求时不要默认开启迁移窗口。

`site/` 可能包含受限制资源，不应整体上传到公共源码或公共 Release。导入模式站点与离线游戏 ZIP 不在本交付包内。
