# 东方神灵庙（th13）移植

C++ / SDL3 / Emscripten 网页实现，行为还原对齐原版 th13.exe（v1.00c）。

- 开发流程：[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)
- 适配验收记录：[ADAPTATION.md](ADAPTATION.md)
- 原版资源由使用者本地提供，仓库不携带任何原版文件

## 构建

```sh
node portable/build.mjs            # 网页运行时（C++ / SDL3 / Emscripten）
node portable/build-harness.mjs    # WASI 无头对照 harness（oracle 比对用）
```

工具链要求见仓库 [docs/adaptation/build.md](../../docs/adaptation/build.md)；本项目提供统一容器环境（工作区 `container/Dockerfile`，本地 podman 与 CI 共用）。
