# 第三方软件与资源

以下说明维护现有来源和许可。依赖安装版本以源码根 npm lock 或独立交付包内 lock 为准；资源旁保留的原始许可文本具有独立用途，不因中文维护文档而删除。

| 组件 | 来源与许可 | 用途及本地位置 |
| --- | --- | --- |
| webaudio-tinysynth 1.1.3 | [上游](https://github.com/g200kg/webaudio-tinysynth)，Apache-2.0 | 无采样 General MIDI 合成，`public/vendor/webaudio-tinysynth.min.js` |
| fflate 0.8.3 | [上游](https://github.com/101arrowz/fflate)，MIT | ZIP 读写，保留 Replay 原始字节；`public/vendor/fflate.min.js` |
| marked 18.0.12 | [上游](https://marked.js.org/)，MIT | 构建 Markdown 帮助页，不作为浏览器 Markdown 运行依赖 |
| font-touhou98 1.0.0 | [上游](https://github.com/thwiki/font-touhou98)，MIT | 字标、阴阳图标；`public/assets/fonts/touhou98.woff2` |
| Yatra One | [上游](https://github.com/google/fonts/tree/main/ofl/yatraone)，SIL OFL-1.1 | 拉丁 UI 子集；`yatra-one-latin.woff2`、`OFL-YatraOne.txt` |
| ChillRoundGothic | [上游](https://github.com/Warren2060/ChillRoundGothic)，SIL OFL-1.1 | CJK Medium/Bold/Heavy 子集；`chill-round-gothic-site-*.woff2`、`OFL-ChillRoundGothic.txt` |
| thcrap | [上游](https://github.com/thpatch/thcrap)，Unlicense | 服务端语言资源发现、验证和有限格式适配 |
| thtk 12 | [上游](https://github.com/thpatch/thtk)，两条款 BSD 式许可 | `thdat`、`thmsg` 提取和编译运营者提供的资源 |
| thprac | [上游](https://github.com/touhouworldcup/thprac)，GPLv3 | 已移植的练习选项及 Replay 元数据适配，不加载 Windows DLL |
| GNU Unifont 15.1.05 | [上游](https://unifoundry.com/unifont/)，SIL OFL-1.1；亦提供带字体嵌入例外的 GPLv2+ | UI 缺字后备；`unifont-site.woff2`、`OFL-Unifont.txt` |
| Font Awesome Brands 7.3.1 | `@iconify-json/fa7-brands@1.2.4`，CC BY 4.0 | `notice-bilibili.svg`、`notice-qq.svg`、`notice-github.svg` |
| Material Symbols | [上游](https://github.com/google/material-design-icons)，`@iconify-json/material-symbols@1.2.86`，Apache-2.0 | language、history、person、warning；触控中的 fullscreen、fullscreen_exit、help、screen_rotation |
| Phosphor 2.1.1 | [上游](https://github.com/phosphor-icons/core)，MIT | 未修改的 regular 房间图标；`room-icons-LICENSE.txt` |

Unifont 原始版权署名：1998–2024 Roman Czyborra、Paul Hardy、Qianqian Fang、Andrew Miller、Johnnie Weaver、David Corbett、Nils Moskopp、Rebecca Bettencourt、Ho-Seok Ee 等，完整文本随字体保留。

`public/assets/notice-touhou-cloud.png` 来自 [Touhou Cloud](https://cloud.touhou.best/) 的服务标识。项目现有授权记录允许用于来源识别；其他提供方没有明确再分发依据时使用文字链接。

thtk 不随自托管包再分发二进制。Windows 构建从上游固定 thtk 12 Release 下载，校验 SHA-256 `f6acc00f377b6537e8d504794aec8445cb1e0d6490d2c89d56b3315677765154`；非 Windows 使用运营者安装的工具。原版资源不因格式工具的许可而获得再分发许可。

源码中执行 `npm ci --ignore-scripts` 后，使用 `npm run vendor --workspace @eagler/web` 重建 vendored JS。字体生成脚本记录上游输入及子集过程。

游戏 Runtime 的 SDL、SDL_image、SDL_ttf、Emscripten、解码器及 ImGui 依赖由相应游戏和工程供应商目录维护：

- [SDL](https://github.com/libsdl-org/SDL)
- [SDL_image](https://github.com/libsdl-org/SDL_image)
- [SDL_ttf](https://github.com/libsdl-org/SDL_ttf)
- [Emscripten](https://github.com/emscripten-core/emscripten)

不要把启动器的署名表当作所有游戏及原版 DATA 的统一授权。项目级依赖边界见根 `THIRD_PARTY.md`。
