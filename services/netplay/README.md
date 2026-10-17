# 联机服务

[English](README.en.md)

本组件拥有房间目录、WebSocket 信令/中继、观战数据转发及遥测。游戏负责输入确认、回滚和玩法，Web 负责房间界面；共同数据来自 `@eagler/contracts`。

## 开发与测试

源码要求与根项目一致，从根目录执行：

```text
npm ci --ignore-scripts
npm run test:services
npm run relay
```

`npm run relay` 会启动服务。部署前明确监听地址、端口、WSS 和实际网络配置；测试运行使用独立进程与目标，不替代公开服务验收。

## 主要配置

| 环境变量 | 用途 |
| --- | --- |
| `EAGLER_NETPLAY_RELAY_HOST`、`EAGLER_NETPLAY_RELAY_PORT` | 监听地址、端口 |
| `EAGLER_NETPLAY_STUN_URLS` | STUN 地址列表 |
| `EAGLER_NETPLAY_TURN_URLS` | TURN 地址列表 |
| `EAGLER_NETPLAY_TURN_SHARED_SECRET` | 服务端动态凭据密钥 |
| `EAGLER_NETPLAY_TURN_USERNAME`、`EAGLER_NETPLAY_TURN_CREDENTIAL` | 成对的静态凭据选项 |
| `EAGLER_NETPLAY_ROOM_LOOKUPS_PER_MINUTE` | 每个来源 IP 每分钟可查询的不同房间数量，默认 12。同房重连及玩家/信令连接共用一次查询 |

隐藏房间仅从大厅目录隐藏，任何知道房码的人仍可加入。新建房间使用 8 位随机房码，兼容旧的 4–8 位房码。房主移除的 client/member 在该房间存续期间不能再次加入；匿名身份可通过换浏览器或清空存储绕过，不能当成账号级封禁。拒绝记录上限为 1024，达到上限后只允许已加入成员重连。

其余重连、缓冲和诊断设置在 `netplay-relay.mjs` 校验。`TH07_*` 名称是既有部署读取兼容，新配置用 `EAGLER_NETPLAY_*`。测试用延迟、丢包和路由偏移选项不作为普通生产默认。

## 独立交付

`bundle.mjs` 的 `buildNetplayBundle(output)` 为调用者拥有的暂存目录生成实际服务闭包与独立 package/lock，保留锁定的 ws 依赖。Web 运营交付工具调用此接口，静态自托管包不内嵌运行中的 Relay。

本轮已验证隔离服务包中的真实 WebSocket 房间连接。服务可达不代表各游戏在所有网络下完成多人验收；还需实际多端输入、观战、退房重开和恢复场景。

[多人流程](../../docs/playbooks/multiplayer.md) · [工程架构](../../docs/ARCHITECTURE.md)
