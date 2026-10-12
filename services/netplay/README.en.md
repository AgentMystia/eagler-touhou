# Multiplayer service

[中文](README.md)

This component owns room discovery, WebSocket signaling/relay, spectator forwarding, and telemetry. Games own input confirmation, rollback, and gameplay; the Web launcher owns room presentation.

From the project root:

```text
npm ci --ignore-scripts
npm run test:services
npm run relay
```

The source workspace uses the root Node.js requirement. `bundle.mjs` creates an independent executable service closure and package lock in a caller-owned staging directory. The service is separate from the static self-host bundle.

The audit exercised a real WebSocket room through an isolated bundled service. This does not establish complete multiplayer gameplay acceptance. Configuration and operational details are in the Chinese README and [multiplayer playbook](../../docs/playbooks/multiplayer.md).
