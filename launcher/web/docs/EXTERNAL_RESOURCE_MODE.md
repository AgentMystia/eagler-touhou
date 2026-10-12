# 外部资源模式

external 将一个已经验证的 Hosted 站点派生为两份匹配产物：完整资源源站保留 DATA、音乐和语言；网页站点保留启动器、Runtime、Package 描述和 App Shell，通过重定向取得资源。

`externalImportSource` 只是手动下载链接，不选择此模式。

## 生成配套站点

在独立包根目录执行，源码则为命令加 `--workspace @eagler/web`：

```text
npm run host:build
npm run package:external-site -- --source=dist/site --runtime-release=runtime-release --output=dist/external-site --profile=web-release-external
npm run verify:server -- dist/site
npm run verify:server -- dist/external-site
```

游戏集合默认沿用 Hosted 的 `deployment.json`；`--games=...` 明确选择子集。输出目录必须与源目录不同，生成成功后会替换受管输出。

正式 Release 的 `hosted-site/` 与 `external-site/` 已经是配套目录。不要用另一 Release 的描述文件或游戏资源替换其中一边。

## 服务器边界

| 角色 | 示例地址 | 承载内容 |
| --- | --- | --- |
| 资源源站 | `https://asset.example.com/` | 完整 Hosted 站点 |
| 网页站点 | `https://play.example.com/` | external 产物及资源重定向 |

网页站点把 `/games/` 和 `/shared/` 请求以 307 转向资源源站，保留路径及查询参数。浏览器直接从源站下载资源；若使用反向代理，则带宽和缓存责任随之改变。

资源源站需要允许网页 Origin 的 CORS，并在 206 和相关错误响应中保持一致。暴露 `Accept-Ranges`、`Content-Length`、`Content-Range`、`ETag`；支持 Range 单段请求并返回正确的 206、长度和区间。缓存键保留 `v`。

完整 nginx 示例见 [nginx-external-pair.conf](../examples/deployment/nginx-external-pair.conf)。修改子 location 的响应头时，检查继承规则，避免丢失 CORS。示例不是服务器或证书安装器。

## 缓存与代次

external 的 Service Worker 管理本 Origin 的 App Shell 和 Runtime，不拥有另一个 Origin 的游戏 Package。HTML/JSON 等入口要能重新验证，按内容标识的资源可以长期缓存。

语言资源使用每作稳定路径和清单哈希作为缓存身份。旧内容寻址 URL 如仍被已发布描述文件引用，可在源站暂时保留；先部署新的同内容稳定 URL，验证后再发布新描述。退役旧 URL 要根据实际支持的客户端和描述代次决定，不新增前端猜路径逻辑。

## 切换、回滚与验证

先使配套资源源站可达，再切换网页站点。发布到新的版本目录，验证后整体切换；不向正在服务的目录逐文件覆盖。若内容身份改变，回滚也应恢复匹配的两边。

```text
npm run verify:deployed -- https://asset.example.com/
npm run verify:deployed -- https://play.example.com/
```

验证器会检查描述指向的文件、重定向、CORS 与字节范围；再从网页站点用真实浏览器安装一个 Package 并启动游戏。命令行响应不能替代浏览器强制执行的 CORS 和存储事务。

从已发布资源代次恢复 external 的维护入口是 `tools/maintainer/recover-external-site.mjs`，操作限制见[恢复说明](https://github.com/YomotsuHisami/eagler-touhou/blob/main/launcher/web/tools/maintainer/EXTERNAL_RECOVERY_LANE.md)。恢复产物不自动成为正式 Release。
