# 运营服务

## 静态站点

部署 `site/` 的完整代次。nginx 示例只演示配置，请使用实际目录、域名和证书。配置修改后先检查，再切换并保留回滚版本。

## Relay

`relay/` 是独立 Node 服务包，使用它自己的 npm lock 安装依赖：

```text
npm --prefix services/relay ci --omit=dev
```

按服务包中的环境示例设置监听和密钥，再由运营者选择进程管理方式。systemd 文件是 Linux 示例，不是通用安装前提。

配置可访问的 WSS 后执行两项实际探测：

```text
node services/verify/verify-public-relay-fallback.mjs wss://example.com/eagler-netplay/
node services/verify/verify-public-targeted-relay.mjs wss://example.com/eagler-netplay/
node services/configure-deployed-site.mjs --site=DEPLOYED_DIRECTORY --relay=wss://example.com/eagler-netplay/
```

是否要求 Relay 由交付 profile 决定。服务探测通过后仍需双端游戏验证。

## TURN 与迁移

需要 TURN 时，按实际网络配置 coturn 和 Relay 共享密钥，并开放选定监听与中继端口。密钥不写入公开站点。

确有 HTTP 到 HTTPS 迁移时使用迁移窗口示例，保持 HTTP `/migrate.html` 可访问。结束迁移后：

```text
node services/configure-deployed-site.mjs --site=DEPLOYED_DIRECTORY --origin-migration=disable
```

再使用最终 HTTPS/HSTS 配置和相应验证器。没有迁移需求时不要套用迁移窗口。
