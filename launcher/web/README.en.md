# Web launcher

[中文](README.md) · [Project overview](../../README.en.md)

This directory owns the browser UI, package installation and storage, Runtime sessions, static-site assembly, and self-hosting tools. Game logic lives in the root game submodules. The independent multiplayer service lives in `services/netplay/`.

## Development

Run from the project root:

```text
npm ci --ignore-scripts
npm run dev
npm run build
```

The root `package.json` defines the source workspace's Node.js requirements. The development server runs the current React frontend. Starting a game also requires its Runtime and legitimately obtained game resources; frontend compilation does not download original game data.

## Maintenance documentation

Maintenance documentation is in Chinese: [architecture](docs/ARCHITECTURE.md), [development and verification](docs/DEVELOPMENT.md), [cache and compatibility](docs/CACHE_AND_COMPATIBILITY.md), [self-hosting](docs/SELF_HOSTING.md), [configuration reference](docs/SELF_HOSTING_REFERENCE.md), and [external resource mode](docs/EXTERNAL_RESOURCE_MODE.md).

See the [project documentation](../../docs/README.md) for cross-game architecture, capability evidence, and release procedures. [Assets](ASSETS.md) and [third-party notices](THIRD_PARTY.md) describe resource provenance and licensing.
