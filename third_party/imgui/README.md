# ImGui 练习界面依赖

此目录供 TH06/TH07 的练习界面编译使用。它是现有 thprac 源码快照中 ImGui 目录的受管副本。

- 声明版本：Dear ImGui 1.82。
- 来源快照：thprac 提交 514768fb932b9aa4b7b34782eb907f8e49cc5a7a，目录 thprac/src/3rdParties/ImGui。
- 保留实际编译使用的核心、FreeType、配置和 thprac 扩展文件；未复制演示程序。
- 本项目改动：imgui_user.h 显式包含 stdint.h，提供 intptr_t 声明。
- 许可证见 [LICENSE.txt](LICENSE.txt)，从 [Dear ImGui v1.82](https://github.com/ocornut/imgui/blob/v1.82/LICENSE.txt) 补齐；thprac 的扩展继续受各源文件的许可声明约束。

这不是完整 thprac 移植。TH_RUNTIME_EXTENSION_CMAKE 指向的关卡扩展仍是独立、显式的构建输入。更新此副本时记录新来源，保留许可，并重新编译 TH06/TH07 的练习构建。第三方代码不参加项目格式化。
