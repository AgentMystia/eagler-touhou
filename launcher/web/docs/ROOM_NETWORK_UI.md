# Room connection diagnostics

`src/launcher/room-network.mts` owns Launcher-only, pre-game probes. It does not join a game run, carry gameplay inputs or change Runtime transport selection.

For each other online seated player, the launcher creates a direct RTC data channel (STUN only) and, when configured, a separate RTC data channel forced through TURN. A third echo travels over the lobby WebSocket through the relay to that same peer and back. All three numbers are peer RTT measured by the requesting client's monotonic clock. “Variation” is the absolute difference from the preceding valid RTT sample, not statistical network jitter or one-way latency.

The relay advertises support and ICE servers in the optional `roomProbe` field of its initial `state` message, preserving the existing first-message contract. Probe messages are targeted to an online seated peer in the same lobby. The relay derives sender identity from the socket, validates the envelope and limits messages to 180 per 10 seconds per connection. Spectators cannot send or receive these probes. Settings, ready flags, launch and Runtime signaling are separate.

The standalone `?diagnostic=1` endpoint can report STUN/TURN credentials even when the deployed relay lacks room-to-room probes. It does not prove this feature is live. Deploy `../../../services/netplay/netplay-relay.mjs` together with `../../../services/netplay/room-probe-policy.mjs`; the initial lobby `state` must include `roomProbe` and the relay must forward targeted `room-probe` envelopes. A frontend-only rollout leaves every peer metric unavailable. Both participants must reload the updated Launcher before judging the peer measurements.

The lexicographically smaller client ID offers RTC connections. The offer/answer is sent before any ICE candidates gathered while setting the local description, so the receiver has the matching connection before candidates arrive. Retry by the other endpoint requests a new offer. Each connection has a generation token; stale async completions and old channel messages cannot update a replacement peer. Polling is every 2.5 seconds. RTT samples expire after 8 seconds and connection setup is bounded at 12 seconds. Leaving a seat, a room disconnect, starting a run or leaving the page closes diagnostics. A missing capability or TURN configuration produces an unavailable state rather than an estimate.

## 验证

Web 目录执行 `node tests/test-room-network.mjs`，项目根执行 `npm run test:services`。实际房间还需双端、直接连接、TURN 和 WebSocket 路径分别验证；本机模型测试不代替公网双端测量。观战聊天和游戏传输与这些开赛前探测各有自己的权限与生命周期。
