# Rollback investigation material

> 固定基线的历史调查，记录当时环境、成功与失败；当前改动须重新验证，不能把本页计数当作现行验收。

This directory contains the detailed investigation records behind
[`../rollback.md`](../rollback.md).

The parent playbook is the maintained engineering guidance. The dated TH07MP/TH08MP
documents here are frozen experiment records: benchmark conditions, rejected
directions, intermediate failures, measured results, and the commands used to
reproduce the investigation. Statements such as "current", "not deployed", or
"pending" describe the state at the date named by that report; they are not the
current project status.

The reports intentionally retain references to temporary `.codex-tmp/`
artifacts and historical build directories because those names identify the
original experiments. Those artifacts are not guaranteed to be present in a
source checkout.

Contents:

- [Initial performance investigation](th07-mp-performance-2026-09-16.md)
- [Input buffering and prediction investigation](th07-mp-buffer-prediction-2026-09-16.md)
- [Mobile-balanced investigation](th07-mp-mobile-balanced-2026-09-17.md)
- [Zero-added-delay investigation](th07-mp-zero-delay-2026-09-17.md)
- [Optional same-origin input delivery](immediate-input-bridge.md)
- [TH08 base Update optimization and TH07 cache reuse](th08-mp-base-update-reuse-2026-09-27.md) — committed candidate, scoped-cache proof, repeatable measurement and unresolved device/Results acceptance.
