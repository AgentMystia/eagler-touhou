# Shared data and protocols

[中文](README.md)

`@eagler/contracts` is an internal workspace for TypeScript data shared by the Web launcher, Node tools, and network services. It owns product metadata, Runtime messages, site/release manifests, resource generations, and multiplayer data formats.

Browser-private models stay in the Web component. C++ simulation algorithms belong to eagler-common. Compatibility readers require an existing persisted or published format.

Source exports point to `src/*.mts`. Web tools generate immutable JavaScript generations from the actual input identity; service distribution bundles its dependency closure. This package is not published as a public npm product.

See the Chinese [architecture](../docs/ARCHITECTURE.md) and [verification guide](../docs/playbooks/verification.md).
