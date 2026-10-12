# 第三方组件与许可

| 组件 | 使用位置与用途 | 许可依据 |
| --- | --- | --- |
| 游戏适配仓库 | 根 `th06/`—`th11/`、`th15/` Git 依赖 | 各子模块原有 LICENSE/NOTICE 与 Git 历史 |
| eagler-common | 公共 C++、浏览器桥接、testkit | [组件说明](eagler-common/README.md)及其 LICENSE |
| Dear ImGui 1.82 | TH06/TH07 练习界面的明确源码依赖 | [来源说明](third_party/imgui/README.md)、[MIT 许可](third_party/imgui/LICENSE.txt) |
| SDL、SDL_image、SDL_ttf 及递归组件 | 游戏平台与资源后端 | 各游戏锁定的 vendored 子模块及其许可证 |
| Emscripten / WASI SDK | 外部编译工具链 | 对应上游工具链许可，不随工程目录复制 |
| Web 字体、图标、JS 依赖、翻译工具 | 当前 Web 启动器及站点构建 | [Web 第三方说明](launcher/web/THIRD_PARTY.md) |

ImGui 来源快照为 `thprac-reallyportable` 的提交 `514768fb932b9aa4b7b34782eb907f8e49cc5a7a` 内 `thprac/src/3rdParties/ImGui`。原有 MIT 许可保留；本地接口适配及所含文件范围见该目录说明。

根工程的 GPL 许可不替代原版游戏与第三方素材的许可，也不授予原版资源的再分发权。
