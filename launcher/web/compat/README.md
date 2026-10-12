# 历史输入兼容

此目录读取已发布的 `eagler-touhou/game-data-pack/1`、`eagler-touhou/offline-game-pack/1` ZIP，以及旧 `eagler-touhou-game-data-v1` Cache Storage/localStorage 记录。迁移成功后删除旧记录；新安装只写 Package Store。

删除兼容前，需要明确最低可升级客户端基线，为跳过基线的用户保留导出/恢复路径，并一起移除启动迁移调用、读取器及对应数据测试。

| 边界 | 当前依据 | 退役条件 |
| --- | --- | --- |
| 旧 Host Manifest 文件/schema | `test-site-manifest-compat.mjs` 验证 404 回退、当前格式错误和别名篡改 | 提高站点/客户端基线并验证升级路径 |
| `EAGLER_FRONTEND=react` | 预检查接受为无操作值，旧 `main` 被拒绝 | 运营脚本完成迁移 |
| 首次使用确认键 | 根 `contracts/src/compat/first-use-notice.mts`；当前 mounted notice 测试覆盖非空、空值和存储失败 | 明确旧客户端窗口后一起移除 |
| 固定 URL Runtime 缓存 | `runtime-generation-reader.js` 读取旧代次；测试检查字节身份和缺件 | 旧缓存窗口结束且有恢复路径 |
| 已发布 `legacy/*.mjs` URL | `path-aliases.mjs` 映射到当前唯一实现 | 与旧包/存储升级窗口一同退役 |
| 只有短 revision 的旧 Package | 对相同 revision/长度的现有对象计算完整 SHA-256，严格匹配后复用 | 不再支持最后一批短 revision 包，完成正常升级周期 |

新的描述和对象都携带完整哈希。短 revision 读取不成为另一生产格式；按完整身份复用对象仍是正常 Package Store 能力。

历史 URL 和协议标记可以保留在读取边界；无需为它们复制旧 UI。详细生命周期见[缓存与兼容](../docs/CACHE_AND_COMPATIBILITY.md)。
