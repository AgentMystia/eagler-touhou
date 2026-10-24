# TH12 多人适配

[开发入口](README.md) · [普通版](../th12/ADAPTATION.md)

源码目录：`ports/th12-mp`。产品：`th12mp`。构建模式：`multiplayer`。多人模式通过独立源码与执行身份接入共用启动器及 Relay。

## 状态所有者

| 内容 | 实现 |
| --- | --- |
| 各席资源、射击、Bomb 与伤害 | 永久 Pilot 与显式 Scope，动画回调保持席位归属 |
| UFO 槽与奖励 | 共用 UFO 管理器，死亡保留槽位，碎片奖励分发到各席 |
| P／残机互赠、幽灵及伤害缩放 | `eagler/gameplay/CooperativeRules.hpp` 与 TH10／TH11 规则 |
| 输入延迟及回滚 | 共用 Netplay Core／SessionChannel／Adonis，原生世界快照 |
| 观战 | 共用阶段协议，TH12 阶段初始化、RNG 与全席资源重建 |
| Replay | 已确认输入存档、构建身份校验、原生重放与进度定位 |
| 房间预加载 | 动态标题、禁止 Demo、Start 复用资源与 WASM |
| 暂停及结束菜单 | 原版确认、排名、姓名与 25 槽 Replay 界面 |

## 自动验证

原生测试使用 C++ 核心；资源测试使用本地合法提取的 `ports/th12/site/assets`。浏览器测试使用正式 WASM、启动器 RPC 与真实本地 WebSocket Relay，配置 25 ms 有序传输延迟。

```sh
cmake -S ports/th12-mp -B ports/th12-mp/artifacts/native-multiplayer
cmake --build ports/th12-mp/artifacts/native-multiplayer --config Release
ctest --test-dir ports/th12-mp/artifacts/native-multiplayer -C Release --output-on-failure
node --test ports/th12-mp/tests/multiplayer-menus.test.mjs
node ports/th12-mp/scripts/check-multiplayer-browser.mjs
```

原生用例验证共享 UFO、自动分 P、P 档位互赠、救援、伤害余数、回滚 RNG、触控及录像校验。资源用例覆盖 Normal／Extra、六种机体、2／3 席、实际弹幕及 Bomb、世界和动画回滚、六面跨关继承。

浏览器用例覆盖双人 0f、三人 3f、直接回滚、自动实测；核对输入帧对应的世界状态、已确认暂停、预加载复用、晚加入旁观、时间轴回看、原版 Replay 槽位与录像重建。报告输出至忽略目录 `artifacts/multiplayer-browser/result.json`。

## 历史维护

普通版来源、多人派生连接点与完整子树登记保存在 `docs/migration/`；目录投影和同步沿 `docs/adaptation/port-sync.md` 维护。修改普通版时，按该作品基线处理多人版的对应修改。
