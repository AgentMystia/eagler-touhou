# Runtime, Launcher and Package Playbook

Status: active

## Purpose

Keep App Shell, immutable Runtime/content, mutable user DATA, Package Descriptor/Store and Launcher product state separate. Route import, update, offline, storage and HTTP migration work without treating a prepared Runtime as a completed game or device product.

## Applicability and authority

Normative ownership rules live in [Architecture](ARCHITECTURE.md). Storage
validation and browser conformance are documented in
[Runtime storage testing](RUNTIME_STORAGE_TESTING.md).

游戏适配义务见根[能力分类](../../../docs/ADAPTER_CAPABILITIES.md)和[接入协议](../../../docs/GAME_ADAPTER_CONTRACT.md)。产品事实来自 `contracts/src/product-catalog.mts`，原版格式与准备方式来自 Web 的 `lib/content-definition.mjs`。新增前端产品按[Web 接入教程](ADAPTING_A_GAME.md)操作；本页负责资源、Package 和会话的拥有关系。

<!-- knowledge-id: K-RUNTIME-001 -->
<!-- knowledge-id: K-RUNTIME-002 -->
<!-- knowledge-id: K-RUNTIME-003 -->
## Normal design

Use this ownership model:

```text
App Shell / Launcher
    → selects and prepares a package
immutable Runtime + content
    → runs the game
mutable user DATA / save / Replay
    → stays in explicit storage roots
```

Service Worker caches and updates the App Shell. It does not own the Runtime filesystem or hide a package Runtime inside SW state. The Runtime uses same-origin HTML/JS/WASM and the App/Launcher manages DATA.

An App Shell replacement first verifies and caches its complete required Launcher set. An idle Launcher then requests activation and reloads itself; an active game or operation defers that page's reload until it finishes. Other old tabs and Runtime iframes do not block activation. Verify both the reloaded current page and a newly opened page can load the new shell and launch a Runtime while an old page remains open.

Resource delivery has three modes. `hosted` publishes game/shared payloads with the site; `external` keeps Launcher and Runtime local while retaining Release Catalog and Package Descriptors for automatic acquisition through same-origin redirect routes; `import` requires a user-selected local Package. External storage topology does not enter Package Descriptor URLs: descriptors retain safe relative sources, and the web-server/CDN layer owns redirects and CORS.

The static `contracts/src/product-catalog.mts` module is the product registry.
The browser-visible `site-manifest.json` describes the deployed Runtime/content
surface, while `release-catalog.json` points at installable Package
Descriptors. There is no third `games.json` protocol. Local import stays
available even when remote fetch fails. Missing files may be non-fatal during
descriptor inspection, but the start gate must complete required files before
calling the Runtime. Package update replaces the installed state through
descriptor/generation atomicity; a partially updated package must not become
current.

Keep ordinary and MP storage roots separate. Keep DATA separate from Replay/save. `runtimeReady=true` means required Runtime dependencies are prepared; it is not a game-function, browser or device PASS. Record local provenance so an old package cannot silently pose as a new one.

Source diagnostics such as Presentation Lab use an isolated build/package
profile. Instrumented WASM, `audit_*` exports, diagnostic workbench files and
evidence reports are not Runtime Release or Package Store inputs and never add
commands to `eagler-touhou/1`.

HTTP-to-HTTPS migration must first run on the old Origin. Preserve a precise migration page without HSTS and use nonce/origin-checked `postMessage`; migrate allowed IndexedDB/localStorage/Cache Storage application data, not arbitrary browser cache or raw OGG cache. Ordinary HTTP requests can redirect after migration handling is available.

## Invariants and pitfalls

- Do not replace `FileSystem::GetPrefPath` with a JS-only mount change when the game uses relative paths; the C++ preference path and IDBFS mount must agree.
- Do not mix App Shell/root redirect with a game data directory.
- Keep generated HTML, managed DATA hooks, package generation and public catalog from the same release generation.
- A stale shell must be rejected by packaging/verification rather than silently repaired by Launcher runtime guesses.
- Show network activity for runtime fetch, package import, OGG/DATA download and App Shell update.
- The feasibility shell is evidence for bring-up, not the final Launcher/Runtime product architecture.
- Do not infer a new title's product contract by copying the previous title's
  `features` object or implementation. Required capabilities have no per-game
  `false`; inherited Launcher features should not be reimplemented in a
  Runtime; format/compatibility mechanisms are not product features.
- Before adding a game-name branch to shared orchestration, classify the
  difference. A real product difference belongs in the product catalog; a
  retail-format difference belongs in a format adapter; historical migration
  belongs in compatibility code. Otherwise prefer the shared protocol.
- Host content preparation is declaration-driven. Reuse an existing
  `PRODUCT_CONTENT[game].hostPreparation` kind for shared formats; change
  `host/lib/site-builder.mjs` only when a genuinely new format/build recipe is
  required.
- A Package mutation may outlive the JavaScript document only as durable
  recovery state, never as a long-lived phantom owner. Large `.data` downloads
  stage a per-game pending generation, so reload/crash must not force the next
  same-game launch to wait the full stale timeout. Current browsers use a
  per-game `navigator.locks` exclusive lock in addition to the IndexedDB pending
  record: browser teardown releases the Web Lock automatically, and the next
  holder may immediately reclaim a pending generation that explicitly records
  Web-Lock ownership. Pending state created by older/non-WebLock clients keeps
  the conservative timestamp/heartbeat fallback so another live tab is never
  overwritten merely because a page reloaded.

## Superseded approaches

Blob HTML as the final carrier, hiding Runtime filesystem state inside a Service Worker, JS-only storage mounts and an unconditional HTTP 301 before migration are superseded approaches. They remain historical evidence for why the current layer boundaries exist.

## Adaptation anchors

- `launcher/web/docs/ADAPTING_A_GAME.md` — adapter
  workflow and Definition of Done.
- `docs/ADAPTER_CAPABILITIES.md` — feature
  classification and verification ownership.
- `contracts/src/runtime-protocol.mts` — typed messages and the current
  command/event/configure requirement tables.
- `contracts/src/product-catalog.mts` —
  per-product declarations only.
- `lib/content-definition.mjs` — original
  content and Host format-preparation declarations; this is where current OGG,
  thcrap-language and prepared-content build recipes are selected.
- Canonical title repositories — Runtime-specific implementation authority;
  historical temp/migration worktrees are provenance only.

## Verification

1. Verify catalog, descriptor, installed generation and required-file list independently.
2. Exercise local import, remote fetch failure, update, reload and rollback/old-generation behavior.
3. Check root 308/nested 200, app.js, WASM MIME, managed DATA hook and package generation.
4. Check storage after reload: ordinary save/Replay, MP roots where applicable, and allowed HTTP migration data.
5. Run the affected Runtime/browser smoke; distinguish static contract, browser and device evidence.

## Deliberately omitted claims

Passing the selected workspace gate does not by itself claim public deployment,
real-device acceptance, long-duration mobile stability or visual correctness.
Those remain browser/device/deployment evidence lanes.
