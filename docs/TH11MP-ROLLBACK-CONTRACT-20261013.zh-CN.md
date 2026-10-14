# TH11MP Launcher 回滚产品契约与验收边界

日期：2026-10-13。配套实现树：`D:\workspace\eagler\worktrees\launcher-th11-rollback`。分支：`experiment/th11-rollback-runtime`，创建基线：`b6aa0ae7798fae5cb0054891f3d9290307e5ccc9`。

本记录只覆盖 Launcher 产品契约、房间快照到 Runtime options 的传递，以及相应测试入口。游戏生产实现位于另一个独立工作树 `th11-rollback-smoothness`。本轮没有修改服务源码、通用协议、其他作品声明、主 Launcher 工作树，也没有推送、部署或修改测试站。

## 1. 为什么需要这几个改动

TH11 原产品声明是 `rollbackLimit: 0`，因此 Launcher 和 relay 会把它当作仅纯延迟的作品。仅给游戏增加回滚实现，房间仍无法真实批准 Direct 会话。此外，两个消费端原来把预测容量上限和缺省值写死为 8；即使房间传入 12，也会在进入 Runtime 前被缩回 8。这会使游戏的固定 P12 合同与 Launcher 不一致。

本次生产改动限定为三个文件：

| 文件 | 改动及目的 |
| --- | --- |
| `contracts/src/product-catalog.mts` | 仅 TH11 设置 `rollbackLimit:12, sendPredictionLimit:12, measuredStartup:true, manualDelayLimit:9`。 |
| `launcher/web/src/launcher/multiplayer-lobby-snapshot.mts` | 预测上限及缺省值使用产品 policy，保留 TH11 的 12。其他作品 policy 的值保持原样。 |
| `launcher/web/src/launcher/multiplayer-runtime-options.mts` | TH11 Direct 固定输出 D0/P12/auto=false；纯延迟输出 P0；明确拒绝直接调用传入的非合同窗口，保留缺省旧调用的 mode1。 |

原 relay 已从同一 catalog 取策略，能够批准 mode0/P12，故不需要修改 `services/netplay/netplay-relay.mjs`。这必须由本轮真实本地 relay 测试验证，源码审查不代替运行证据。

## 2. 房间字段与 bridge 字段不是同一个数值合同

`room.predictionLimit` 是房间产品容量声明；`netplayPredictionLimit` 是启动游戏时的运行模式参数。纯延迟时前者仍为 12，后者为 0。

| 场景 | start / room.adonisMode | room.predictionLimit | bridge mode | bridge inputDelay | bridge predictionLimit | bridge auto |
| --- | --- | --- | --- | --- | --- | --- |
| Launcher 开启回滚 | 0 | 12 | 0 | 0 | 12 | false |
| 关闭回滚、手动延迟 | 1 | 12 | 1 | 0–9 | 0 | false |
| 关闭回滚、自动测量 | 1 | 12 | 1 | 0（测量前） | 0 | true |
| 兼容接收到的 Launcher mode2 请求 | relay 归一为 0 | 12 | 0 | 0 | 12 | false |
| 旧调用没有显式 adonisMode | 不经此项改变旧调用 | 可缺省 | 1 | 保留纯延迟校验 | 0 | 沿用合法调用值 |

Launcher 产品仍仅提供 Direct 与纯延迟两个选择。游戏 SessionSetup v6 的原生 mode2 能力不代表本轮 Launcher 新增了第三个产品模式。Launcher 对请求 2 的归一为 0 沿用既有产品逻辑。

Direct bridge 的明确预测窗口必须是 12；P0、P1–11、P13、非整数、NaN 都拒绝。未提供窗口则用产品的 12。纯延迟允许房间传来的 P12 和旧调用显式 P0，最终输出 P0。已有房间 URL 优先识别真实作品，不能用其他作品 constraints 绕过 TH11 窗口约束。

通用 relay 的区间校验仍允许产品容量内的 P1–11；本轮没有擅自把通用服务改成 TH11 专用定长协议。TH11 正常 UI 始终发送 12，Runtime options 边界对非 12 窗口显式拒绝，防止静默变成另一个原生会话。通用快照 normalizer 保留既有无效值回落规则；它不承担固定 P12 的最终准入责任。

纯延迟的 `predictionReserve:2` 启动请求仍可沿用原测量接口；纯延迟最终测量结果的 reserve 必须是 0，现有断言保留。Direct 不伪造 EBTM 测量结果。

## 3. 测试覆盖与明确未覆盖项

`launcher/web/tests/test-th11-multiplayer-policy.mjs` 使用实际产品、生成后的 options/normalizer 模块和实际 relay 进程。覆盖：

- TH11 机体、Extra、2P/3P、挑战规则及观战包标识保持；TH08/09/10 原 send policy 仍是 8。
- Direct 0、兼容请求 2、纯延迟 1、旧缺省调用、非法窗口、非法时序、观战 Runtime options。
- 房间 P12 穿过 normalizer；纯延迟 bridge P0 与 Direct bridge P12 的区别。
- 真实隔离 loopback relay：2P Direct、3P Direct、2P 自动纯延迟、3P 手动纯延迟、2P 请求 2 归一 Direct。
- 真实 relay 的开始批准、挑战变化清除准备状态、非法时序拒绝、纯延迟测量结果确认、Direct 拒绝虚假测量结果。
- 把真实 start.room 再交给 actual normalizer 与 Runtime options 构造，验证跨边界值，不只单独检查 catalog。
- 每个场景使用独立 relay，测试只结束自己启动的 relay 与 socket，并等进程 close 后才进入下一场景。

`launcher/web/tests/ui/multiplayer-services.test.mjs` 的 TH11 专项用实际 room service 配合模拟 socket，验证房主切换模式、2P/3P Extra、start 请求与 bridge 值、P8 房间被拒绝。

`launcher/web/tests/ui/room-views.test.mjs` 的 TH11 专项用实际 React 组件配合合成 DOM，验证回滚按钮可用与模式切换。该证据不代表浏览器布局或游戏表现。

`launcher/web/tests/browser/test-th11mp-launcher.py` 继续作为原有纯延迟浏览器验收入口：现在由真实房主 UI 显式关闭回滚，再保留原有测量、P0 和自动测量断言；没有把原纯延迟脚本改名后冒充 Direct 验收。此脚本本轮尚未运行。

以上测试不能证明：游戏 World 恢复正确性、WASM bridge 与 v6 编码实际联通、GPU 上传/停显、rAF 调度、4× CPU 流畅度、真实公网 direct/relay 差异、高延迟/网络尖峰、合成 120Hz 触控移动。上述证据由游戏实现树分阶段提供。不得由 Launcher 测试承诺实际游戏帧率或性能改善。

## 4. 隔离依赖准备

新 worktree 初始没有 `node_modules`。不能将整目录 junction 到主树，因为其中 `@eagler/contracts` 会指向主树，使 relay 和 bundle 读取错误 catalog。

新增 `tools/prepare-th11-rollback-dependencies.mjs` 仅复用本地已有第三方包，给 companion 建独立根目录，并明确映射：

| 包 | 指向本 companion |
| --- | --- |
| @eagler/contracts | contracts |
| @eagler/web | launcher/web |
| @eagler/netplay | services/netplay |

脚本在写入前核对两树 lockfile 一致、workspace 列表与包名一致、整个链接计划没有未知文件；不覆盖现有文件或身份不同的链接。已存在且目标完全相同的链接允许恢复执行。不会安装包、访问网络、运行构建/测试、修改原依赖目录。准备结束分别从 web 与 relay 的解析上下文执行 `require.resolve` 并检查 `realpath`，确保实际 catalog 来自 companion。收据包含解析路径、lockfile SHA-256、准备脚本 SHA-256，保存到本树 `node_modules/.th11-rollback-dependencies.json`。

这不是可发布依赖包；本地链接仍依赖原第三方包目录存在。禁止在这些共享第三方目录上安装或修改依赖。若 lockfile 不同，脚本先停止，由主代理审查差异，不静默套用。

## 5. 主代理的最小串行命令

工作目录固定为 `D:\workspace\eagler\worktrees\launcher-th11-rollback`。本分工代理没有执行以下命令，也没有把待跑项目记为已通过。

```text
node tools/prepare-th11-rollback-dependencies.mjs --source-root D:/workspace/eagler/eagler-touhou
node --input-type=module -e "import('./launcher/web/lib/client-services-build.mjs').then(m=>m.ensureClientServicesBuild({force:true}))"
node tools/test-th11-rollback-contract.mjs --output artifacts/th11-rollback-contract-01
```

新增 wrapper 不安装依赖、不重复构建，按顺序运行上节的 policy/真实 relay、通用 Runtime options 回归、TH11 UI 三条命令。输出目录必须是本树 artifacts 下的全新目录；任一命令失败停止后续，保留完整 stdout/stderr、命令 header、退出状态及 receipt.json。receipt 保存 HEAD/分支/工作树状态、运行时版本、依赖真实解析路径、相关源码和实际生成模块的逐文件 SHA-256，并比较运行前后身份；源码或生成模块变化也判失败。UI 测试中原有临时 esbuild bundle 属于其测试准备，本 wrapper 不调用 client-services 构建。

先准备再编译 typed client services：专项测试静态导入生成后的模块，新树直接执行会缺少 cache。编译 helper 同时建立 contracts 构建身份。Node 版本应满足现有仓库要求 `^24.15.0 || >=26`。

现有 `config/runtime-builds.json` 已支持 TH11 独立 multiplayer 源构建，保留 THCRAP=true、THPRAC=false；没有必要为本次协议改它。后续 Runtime 包验收必须显式选择 `th11-rollback-smoothness` 源树，并保存真实产物身份，不能误用主树或旧预构建产物。

## 6. 当前证据状态

- 文件已由 nig 写入并读回；所有构建、测试和浏览器执行留给主代理串行。
- 主代理已执行依赖准备，nig 命令 `TAzlrVXS4-8-juzIdQ_5k5Sl` exit0；已读回收据，准备时间 `2026-10-13T08:43:31.607Z`，新建 363 个链接。web/relay 的 catalog realpath 均明确指向 companion 的 `contracts/src/product-catalog.mts`。
- 准备收据的 package-lock SHA-256 为 `d3914a44604004fc4599584a2424f76efb57b1098d792abbfd84bd9ded2ea67b`，准备脚本 SHA-256 为 `f562189ca3a5c2b0099b838a0f7f8120263e7aa80f19802fedc3e8aeab626fc1`。该结果只证明本地依赖身份准备，不证明测试通过。
- 定向测试 wrapper 由主代理执行，contract-02 已正式通过；本分工代理独立读取完整收据和各日志，详见第 7 节。
- 生产范围冻结于三个文件；没有修改任何服务源码。
- 不因本记录或静态审查宣称 TH11 回滚可交付，后续以真实构建和逐层验证为准。

## 7. 定向验收正式记录：contract-02

主代理执行命令：

```text
node tools/test-th11-rollback-contract.mjs --output artifacts/th11-rollback-contract-02
```

nig 命令身份：`JgqGMj5XrFzhtwAakWBnf0G-`。命令轮询曾遇 FRP HTTP404；该传输故障不代替进程状态。现已独立读取完整磁盘收据及三项 stdout/stderr，收据完成时间与各子进程退出码齐全。

- 开始：`2026-10-13T08:53:37.143Z`；结束：`2026-10-13T08:53:48.657Z`。
- `passed=true`、`sourceStable=true`、`generatedStable=true`、`dependenciesStable=true`；`notRun=[]`。
- 三项命令均 exit0，三份 stderr 均为空。
- HEAD 仍为 `b6aa0ae7798fae5cb0054891f3d9290307e5ccc9`，工作树包含本轮未提交改动。
- 实际 Node 为 `24.13.0`。仓库 engines 声明 `^24.15.0 || >=26`；本次证据准确限定于实际执行版本，没有改 engines，也没有把测试通过描述成已验证声明版本的全部兼容性。

| 子项 | 日志中实际通过的内容 | 执行时间 |
| --- | --- | --- |
| 01-th11-policy-relay | 五个真实隔离本地 relay 场景逐项 PASS：2P Direct、3P Direct、2P 自动纯延迟、3P 手动纯延迟、2P 请求 mode2 归一 Direct；实际模块和 EBTM/EBNP 断言通过。 | 6.621 秒 |
| 02-runtime-options-regression | 通用 Runtime options 回归 PASS；2P/3P、产品人数/难度/机体约束、显式观战角色通过。 | 0.162 秒 |
| 03-th11-room-ui | 两个定向 UI 用例通过，tests=2、pass=2、fail=0、cancelled=0、skipped=0；实际 service 的 2P/3P Extra 与模式切换，以及房主回滚开关。 | 2.846 秒 |

收据位置：`artifacts/th11-rollback-contract-02/receipt.json`。同目录保存各子项 `.stdout.log`、`.stderr.log`、`.header.json`。

身份摘要：

| 范围 | 数量 | SHA-256 |
| --- | --- | --- |
| 相关源码 manifest | 407 个文件 | `9e8e749b69465969c21747b54a65dd6efed00b43ac03171f622421330932b737` |
| 实际生成模块及 client-services 状态 | 84 个文件 | `6778b2659e6d0f755172e395e9ad3fa8cbfd4161265b58504d9f9a412c33b3cd` |
| policy/relay stdout | 438 字节 | `fbfdd2d9f8da348209792d9ef15945aa7e8d76ef03a5c91132de822233363dd8` |
| 通用 options stdout | 150 字节 | `6c0e974a3291624796841e265a60844fe06b47db1acc952ce1fab4ca2a052ef1` |
| TH11 UI stdout | 293 字节 | `2d736d557a01b45f76baa6b3bbae86c181163d6cfa68fa6100575621fc96d811` |

之前的 `contract-01` 保留为失败证据：nig `9px3JpfjF10d53T_Ic1_U04_`，第一项在 room9111 创建时收到 `rate-limited`，后两项未运行。原因是增加五个场景后沿用一个 relay，先前房间一直保留，正常触发同 IP 房间配额。本次只修测试生命周期：每个场景创建独立 relay，finally 关闭自己的 socket、终止自己的进程并等待 close，再进入下一场景。未放宽服务准入规则或通过环境变量提高配额。原失败收据不删除、不改写。

typed client-services 编译先前由主代理执行，nig `iKDEl1azlqf71JyHHj2qtS7f` exit0；本轮 wrapper 没有重复调用该构建。上述完成记录证明 Launcher 契约及本地 relay 批准路径，仍不等于真实游戏浏览器或性能验收通过。


