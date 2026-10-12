# 服务器配置示例

本目录演示生成站点需要的 HTTP 行为，不安装 Web 服务、证书、CDN 或防火墙。

- `nginx.conf`：MIME、缓存、根入口、历史挂载和可选 WebSocket 代理。
- `nginx-external-pair.conf`：Hosted 资源源站与 external 网页站点的重定向、CORS 和 Range。

运营者按实际域名和目录修改配置，验证完整代次后切换，并保留可回滚版本。部署后使用 `npm run verify:deployed -- https://example.com/` 检查真实响应，再执行浏览器安装和游戏启动验证。

配置定义见[自托管参考](../../docs/SELF_HOSTING_REFERENCE.md)。
