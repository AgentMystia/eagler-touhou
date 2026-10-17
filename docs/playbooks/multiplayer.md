# Multiplayer Playbook

Status: active

## Purpose

Route multiplayer, rollback, spectator, MP Replay and MP Launcher work without reviving the withdrawn early multiplayer path or mixing network transport, deterministic simulation and product packaging.

For rollback **optimization** rather than multiplayer architecture, use
`rollback.md`. It preserves the complete TH07 journal/input/snapshot/mobile
optimization exploration, including failed experiments and zero-delay evidence.

## Applicability and authority

Use the source and ancestor explicitly selected by the task. Source and merge ownership follows [Adaptation source and workspace](adaptation-worktrees.md); build entries do not create or switch worktrees.

- TH08 and TH10 keep ordinary `eagler` and Multiplayer implementation histories
  permanently separate. Both repositories use `eagler-mp` as their persistent
  MP source branch. Keep this branch separate from `eagler` and retain it after
  shared fixes have been transferred. Their Runtime owners remain the
  canonical `th08-eagler` / `th10-eagler` repositories. Select the branch for the
  requested product before changing or building source. Carry shared fixes
  across with individually reviewed commits where applicable, preserving each
  target's ancestor and gameplay contract. Never merge the complete MP
  implementation into the ordinary branch or replace an ordinary Runtime with
  an MP build. MP topic integration targets the corresponding MP branch.
- TH06MP and TH07MP provide the reusable architecture and test evidence for the
  products that currently declare Multiplayer. TH09MP uses native two-player
  versus selection, exact confirmed menu input and its title-specific
  rollback/delay session over the shared `eagler-common` browser transport. Products declaring
  measured startup select pure delay or hybrid rollback before frame zero;
  see [Measured Multiplayer Startup](adonis-adaptation.md).
  TH09 spectator history, admission and playback must be verified together;
  room facts stay in `product-catalog`, and the experimental late-join candidate
  is not a blanket deployed-coverage claim.
- TH09MP touch movement ships the gesture's absolute field target (motion modes
  2/3 of its input frame) rather than a velocity sampled on the sending machine,
  and the Launcher keeps game keys flowing from the realm that owns focus.
- Titles that do not declare `multiplayerRuntime + multiplayer` are outside
  this profile; do not describe that absence as unfinished adapter work.
- Multiplayer itself is an **optional product capability**. Once a product
  declares `multiplayerRuntime + multiplayer` in the shared product catalog,
  the profile-required Multiplayer behaviors are mandatory; do not add a
  second per-title feature switch for them.
- Browser transport claims come from reproducible transport and Launcher tests. A room connection or focused browser PASS is not human cross-device acceptance.

<!-- knowledge-id: K-MP-001 -->
## Normal design

For products declaring `inputTiming.measuredStartup`, follow
[Measured Multiplayer Startup](adonis-adaptation.md) for pure/hybrid timing,
frame-zero calibration and acceptance. The zero-added-delay policy below is
specific to TH06MP/TH07MP.

Treat ordinary and MP variants as separately identified products whose
relationship comes from `product-catalog`, not from title-number branches.
Use separate runtime/build identity, URL or package identity, room/session state
and Replay semantics where required. Shared UI/service helpers are the default;
product facts such as loadout/difficulty bounds stay in the catalog.

The TH07MP reference layering is:

1. protocol: version, HELLO/READY, frame numbering, input, confirmation and targeted relay envelopes;
2. synchronization: deterministic simulation, input prediction, bounded rollback and Stage/Result lifecycle;
3. transport: WebRTC full mesh with WebSocket fallback and separate control/input semantics;
4. browser: signaling, ICE/TURN, reconnect and route state;
5. game adapter: the owners for GameManager, Enemy, Bullet, Item, Effect, ANM, ECL, Replay and input.

Use a frame-0 barrier, bounded direction prediction, confirmed frames, limited catch-up and a sparse/fixed-arena journal. Full-world snapshots were measured at roughly 19 MB each and are not a per-tick transport strategy. Relay forwards targeted envelopes; it does not simulate the game.

For the current TH06/TH07 production profile, there is one player-facing timing policy: zero added local input frames with full rollback on every endpoint. The generic netplay core may retain input-delay primitives for tests and future evidence-backed designs, but the shared Launcher does not expose a title-specific buffered/stability mode. Do not reintroduce the retired TH07 mobile/desktop asymmetric policy merely for backward compatibility.

Spectators consume confirmed history/checkpoints without taking a player seat or submitting gameplay input. Whether admission is start-only or supports late join depends on the actual Runtime/history stream. TH09 late-join timeline playback is an experimental candidate and needs the complete already-started-room frontend path verified. Runtime exit must not accidentally destroy an active room that Launcher still owns.

<!-- knowledge-id: K-MP-002 -->
## Invariants and pitfalls

- One logical frame samples input once. Retransmit, retry and prediction never resample a device.
- A lockstep payload must not carry a value derived from the sender's simulation
  state at send time. Sampling happens `lead` frames before the frame that
  applies it, so a derived velocity aims from a position the receiver has not
  reached: TH09's touch gesture shipped that way made a dragged player orbit the
  finger forever. Ship the absolute target (or sample on the applied frame) and
  let every peer derive the frame-local value identically.
- Delivery, prediction, confirmation and rollback each have explicit owners.
- Restart creates a new generation/session identity; a visible canvas is not proof that transport is healthy.
- Reconnection waits are bounded; an ICE restart requires missing health or confirmation progress, not just a transient `disconnected` state.
- MP Replay uses the documented local-player offset and isolated MP storage; do not infer ownership from visual seat position.
- Keep rollback/presentation separate. Remote smoothing and alpha are draw-side; rollback changes logical state only.
- Do not restore the withdrawn `TH_ENABLE_MULTIPLAYER`, `MultiplayerRuntime.*`, `g_Player2` or old LCConnect path.
- Do not add or expand hash-based gating to hide a semantic mismatch. Compare the divergent owner/state and preserve existing correctness checks.
- Relay/TURN is shared service infrastructure. New configuration uses the
  `EAGLER_NETPLAY_*` namespace. Historical `TH07_*` relay/TURN variables are
  compatibility aliases only and must not be copied into a new title-specific
  namespace.
- A diagnostics/fallback room is product-neutral. Do not encode `th07mp` (or
  another title) merely to reach shared signaling/ICE health checks.

## Superseded approaches

The early TH06 LCConnect/`MultiplayerRuntime.*`/`g_Player2` route is withdrawn. Full-world per-tick snapshots and decorative rollback state are also superseded by bounded sparse ownership and logic-only rollback.

## Code and system anchors

### Public room directory

The React `lobby.html` route is a separate directory page; the launcher's Multiplayer
shelf links to it. Each supported game's options panel links to `lobby.html?game=<mp-product>`
using the catalog's game-to-Multiplayer mapping. The directory keeps that filter.
The Launcher and directory opt into a short native document crossfade, including
history navigation where supported; room transitions use one surface without
nested dock entry animations. Reduced motion suppresses both.
Room departure commits immediately for both the return button and system Back;
the destination animates without waiting for an outgoing-room fade timer.
The room button invokes departure directly, without waiting for `history.back()`
or `popstate`. Directory returns replace the URL with the matching game's lobby;
`document.referrer` cannot identify the preceding entry after settings/panel
navigation. Options returns clear panel history flags and modal/inert state.
The directory reveals its first screen only after a relay result (or explicit
connection failure) and critical fonts/background/visible covers have settled,
with a bounded visual wait matching the Launcher. Initial rows do not replay
their insertion animation at reveal. A standalone preload failure surface keeps
reload/back available if the page module fails to load.
`app/services/lobby-directory.ts` consumes real relay snapshots.
`app/components/directory/` renders the shared directory components and uses the
React room route for creation/joining. Browsing the
directory never creates a room or takes a seat. No generated mockup is shipped
as a UI asset. Both the page and its dependency closure belong to the sealed React artifact.

P1 can select public/private discovery and disallow unlimited-speed touch
movement. Private means hidden from the directory, not password protection;
room-code joins still work. Creation applies these rules before the first
snapshot, including a private host whose seat is pending a movement change.
The UI calls this visibility option "Unlisted" and explicitly warns that anyone
with the code can join. New room codes contain eight cryptographically random
digits; legacy 4–8 digit joins remain supported.
Host removal rejects both the removed client and its anonymous browser member
for this room's lifetime. It is not an account ban: a different browser identity
can evade it. Removal history holds at most 1024 entries; at that limit fresh
members are refused while admitted members can still reconnect. The relay also
limits distinct room lookups per source IP before checking room existence.
Changing the movement rule invalidates readiness. Restricted rooms require an
explicit permitted movement mode at seat admission, readiness and game start;
the Launcher offers normal touch / joystick in the shared decision dialog and
blocks switching back to unlimited movement while seated. This is a client
settings agreement enforced by the relay, not anti-cheat against modified clients.
Player avatars show a compact control-mode caption in both room and directory:
unlimited touch / touch / normal (touch disabled or either joystick mode).
The relay derives this from declared touch settings, advertises support, and
publishes updates. Empty seats and legacy peers with unknown settings have no caption.

`server/room-directory.mjs` is owned by the shared netplay relay. The same relay
URL from Site Manifest accepts `?directory=1&member=<anonymous-browser-id>` and
sends version-1 `directory` messages. Snapshots expose room code/product,
capacity, occupancy, difficulty, ready status, spectator count and **initials
only**. They exclude internal client IDs and full display names. `refresh`
messages optionally select a product; responses are bounded to 200 rooms.
Snapshots echo the selected `product`. The directory shows a loading indicator
until the requested filter's snapshot arrives, and only then shows an empty
state when there are no rooms. Earlier filter replies cannot settle a new filter.

- The new Launcher shares one anonymous local-storage membership ID across
  titles and tabs, while retaining per-tab client IDs. The relay rejects a
  second active room/tab (4009); an exact-session reconnect replaces the prior
  socket (4008). This prevents accidental multi-tab occupancy, not deliberate
  bypass via another browser, cleared storage or a legacy client.
- Directory navigation uses explicit `create`/`join` intent. Creation conflicts
  and vanished join targets fail with 4007 instead of recreating stale rooms.
- Socket ping/pong runs every 30 seconds, with the next missed heartbeat
  terminating the connection. Existing room reconnect grace remains 12 seconds.
  An explicit departure releases the seat immediately.
- A force-closed mobile page may retain a server-side socket until heartbeat
  expiry. The directory provides an explicit “leave old room” action scoped to
  its anonymous member and the exact observed session token. It releases that
  member's seat, spectator admission and game/signaling transports, never another
  member or a newer session. The page unlocks only after server confirmation;
  old relays show a compatibility hint rather than pretending to release locally.
- Waiting rooms with no action for 5 minutes are hidden; after 30 minutes they
  are closed (4004), with maintenance running every 30 seconds. Room operations
  and throttled trusted interaction refresh activity; network probes/heartbeat
  do not. Live game/signaling/spectator transports are exempt from idle expiry.
  Returning from a completed game starts a fresh waiting-room activity window.
- Room activity is sent only after the relay advertises `roomDirectory.version`
  in its handshake, so old relays remain compatible with the normal room UI.
  An old relay cannot serve the directory: the page displays that limitation.
- Publishing the frontend alone is insufficient to enable room discovery.
  Ship the updated relay together with `room-directory.mjs` and activate it in
  the service's normal maintenance window. Do not restart a live relay as part
  of frontend preview work.

The directory's form sheets own a browser-history entry. Entering a room closes
that entry first; both the room return action and system Back lead to the
directory. Room sessions and shared room settings remain owned by the Launcher.

### Existing multiplayer anchors

- sibling `eagler-common/`: shared cross-title netplay/runtime authority.
  Generic session, transport, core, input ownership, journal/rollback storage,
  browser catch-up/time-sync pacing, confirmed-frontier liveness and reusable
  fault-injection primitives converge here. Title repositories retain small
  protocol/input/transport config seams, rollback-state inventories, canonical
  hashes, gameplay lifecycle and title acceptance fixtures. Do not build a
  universal title driver merely to eliminate a few lines of adapter glue.
- `contracts/src/product-catalog.mts`: optional Multiplayer
  declarations and per-product room/loadout bounds.
- `contracts/src/adapter-capabilities.mts`: profile-required
  Multiplayer obligations and legacy compatibility classification.
- `services/netplay/netplay-relay.mjs`: shared relay policy; derives room
  product policy from the declared product prefix and uses generic service
  configuration.
- `src/launcher/multiplayer-relay-url.mts` and
  `network-diagnostics.mts`: shared transport URL/diagnostics ownership.
- `tests/test-netplay-relay-product-policy.mjs` and
  `test-netplay-service-config.mjs`: product-policy and deployment-namespace
  gates.
- `tests/test-th09-mixed-entry-launcher-browser.py`
  (`npm run test:th09-mixed-entry:browser --workspace @eagler/web`) and `tests/test-th09mp-launch.py`
  (`npm run test:th09mp-launch:browser --workspace @eagler/web`): TH09 room interop in both directions
  (in-game title dialog hosting the Launcher card joining, and the reverse) and
  a real two-Runtime versus match over the shared transport, respectively. The
  launch gate runs the muted music path too, and can take its content from an
  offline package with `--package-zip=PATH`.
- `tests/test-multiplayer-spectator-launcher-contract.mjs` and
  `tests/test-multiplayer-replay-launcher-contract.mjs`: Launcher product
  contracts.
- TH06MP/TH07MP title repositories: title-specific rollback inventories,
  gameplay semantics and transport acceptance evidence.

## Verification

Check in this order:

1. ordinary/MP build and storage identity are isolated;
2. protocol/version, frame-0 barrier, input ownership and prediction bounds;
3. 2P and 3P core, restart/death/Result, Replay and spectator backlog;
4. WebRTC, WebSocket fallback, Relay/TURN candidate and route state;
5. Launcher card/room/exit behavior;
6. browser-focused evidence, then clearly labeled human/device evidence.

Record first divergent logical frame and owner state. Do not substitute a screenshot, a room join or a source-only contract for end-to-end acceptance.

When the failure is performance rather than protocol/lifecycle, follow the
measurement order and exact-byte/canonical gates in `rollback.md`; do not infer
rollback cost from average FPS alone.

## Deliberately omitted claims

This playbook does not make multiplayer mandatory for a title and does not
promote focused protocol/browser tests to cross-device human acceptance.

## 当前服务与验收入口

### 三层边界

[联机服务](../../services/netplay/README.md)拥有房间、连接和转发；[Web 会话](../../launcher/web/docs/ARCHITECTURE.md)拥有用户选择、连接呈现与 Runtime 生命周期；游戏/common 拥有实际输入、确认、回滚和游戏状态。房间“已连接”不代表游戏已同步，编译成功也不代表联机可玩。

产品登记包含 TH06—TH11 多人入口，TH15 不提供多人。TH08/TH10/TH11 的独立多人源码见 [Runtime 构建](runtime-build.md)。所有状态以实际源码、Runtime 清单和运行证据核对。

### 调整服务或协议

```text
npm run test:services
npm run relay
```

服务环境变量、启动参数以 `services/netplay/netplay-relay.mjs` 及交付示例为准。中继和遥测交付由服务自己的 `bundle.mjs` 打包，使用其依赖闭包；不从 Web 目录手动挑若干服务文件。

变更房间消息时检查身份、房间代次、成员/旁观者权限、断开、重连、过期消息和清理。协议变更必须同时检查 Web 消费者、Node 服务和游戏桥接，不能只改产品登记表。

### 实际联机验收

记录双方/多方的源码提交、common 版本、Runtime 代次、角色、关卡、网络路径和输入。先验证固定逻辑、重新开始、断连退出、回放与普通单人隔离，再做抖动、延迟或性能调优。

WebSocket 回环与房间单元测试不代替真实 WebRTC/TURN、移动网络和跨设备长局。本轮仅服务测试与隔离中继 WebSocket 会话有执行证据，未新增全作品公网联机完成结论。

### 排错

先确认当前使用的是普通还是多人产物，以及接收消息的 game/epoch 是否匹配。然后区分房间协议失败、传输失败和游戏逻辑发散。不要通过延迟丢帧、跳过 trace、改参考状态或无限重试掩盖问题。
