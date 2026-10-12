# eagler-touhou self-host bundle

[中文](README.md)

This bundle contains the prebuilt Web launcher, Host tools, and a Runtime Release. It does not contain original game DATA. Prepare your own resources and required fonts for the games included in this Runtime Release.

```text
npm run host:doctor
npm run host:build
npm run verify:server -- dist/site
```

Operator documentation is in Chinese: [setup](docs/SELF_HOSTING.md), [configuration](docs/SELF_HOSTING_REFERENCE.md), [external resource mode](docs/EXTERNAL_RESOURCE_MODE.md), and [third-party notices](THIRD_PARTY.md).

Building does not upload or deploy a site. Preserve your inputs, configuration, saves, and previous working deployment.
