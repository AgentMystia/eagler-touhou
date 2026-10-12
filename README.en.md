<h1 align="center"><img src="docs/assets/eagler-touhou-wordmark.svg" alt="EAGLER TOUHOU" width="312"></h1>

<p align="center">Web adaptations, a shared launcher, and multiplayer infrastructure for Touhou Project games.</p>
<p align="center"><a href="README.md">简体中文</a> | English</p>

This repository retains its existing history and coordinates the Web launcher, shared contracts, network services, and source dependencies for TH06–TH11 and TH15. Games run as WebAssembly Runtimes. The launcher manages resources, settings, touch input, saves, and Replays. Original game data must be supplied separately.

See the [capability and verification table](docs/TOUHOU_PROGRESS.md) for current support. Registered features, successful builds, and verified gameplay are different evidence. TH09 still declares its Runtime incomplete. TH20 retains its existing catalog and prebuilt integration; its source is outside this engineering audit.

![Launcher interface](docs/assets/readme/launcher.webp)

## Use and self-hosting

Use a deployed site to install available resources or import a game package you own. The [FAQ](launcher/web/docs/FAQ.md) and [self-hosting guide](launcher/web/docs/SELF_HOSTING.md) cover the current Web frontend and its Hosted, External, and Import resource modes.

Offline use requires installed resources and cached Runtime code. Browser storage, site versions, and secure-context requirements still apply. Multiplayer also requires a compatible Runtime and configured network services. TH15 has no multiplayer mode.

## Development

The source workspace requires Node.js 24.15 or later within Node 24, or Node.js 26 and newer. Install Python, CMake, Ninja, and Emscripten when the selected task needs them.

```text
npm ci --ignore-scripts
npm run doctor
npm run dev
npm run build
npm run check
```

Frontend development does not require original game data. To build a game:

```text
npm run dependencies -- init th15
npm run runtime -- th15 --emsdk "<SDK directory>" --font-root "<font directory>"
```

See the Chinese [development guide](docs/DEVELOPMENT.md) and [Runtime build playbook](docs/playbooks/runtime-build.md) for complete inputs, TH06/TH07 asset handling, and explicit multiplayer source selection.

## Repository

| Directory | Responsibility |
| --- | --- |
| `launcher/web/` | Web frontend, site assembly, and deployment tooling |
| `contracts/` | Protocols and data formats shared by frontend, services, and tools |
| `services/netplay/` | Rooms, signaling, relay, and telemetry |
| `th06/`–`th11/`, `th15/` | Root-level Git submodules producing game Runtimes |
| `eagler-common/` | Shared C++ support, browser bridges, and testkits |
| `tools/`, `config/` | Project commands and dependency/build declarations |
| `docs/` | Project architecture, capability tracking, audit, and playbooks |

Other platform launchers are not implemented and no shell technology has been selected. Maintenance documents are kept in Chinese; this README and the Chinese README describe the same project.

## Contributing and licensing

Start with the [documentation index](docs/README.md) and [engineering audit](docs/ENGINEERING_AUDIT.md). Describe the affected behavior, actual verification, and remaining limits. Source-text snapshots, full stylesheet hashes, and historical object-file identity are not substitutes for behavioral tests.

The project's code uses [GPL-3.0-or-later](LICENSE). Each game and third-party component retains its own license. See [third-party notices](THIRD_PARTY.md) and [resource handling](ASSETS.md). Original data, personal saves, toolchains, and deployment configuration are not public source files.
