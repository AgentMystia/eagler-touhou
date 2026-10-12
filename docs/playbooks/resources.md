# 资源与语言

## 输入拥有者

原版 DATA、音乐和个人存档不进入源码仓库。字体和第三方图像需要保留来源与许可证；站点可发布的文件清单见 [Web 资源说明](../../launcher/web/ASSETS.md)。

| 输入 | 声明/实现 |
| --- | --- |
| 各作原版文件、预制 DATA、OGG 目录 | `launcher/web/lib/content-definition.mjs` |
| 字体、DATA 和资源扩展组装 | `launcher/web/host/lib/site-builder.mjs` |
| 封面和站点图标 | `launcher/web/scripts/prepare-host-artwork.py` |
| thcrap 语言准备 | `launcher/web/integrations/thcrap.mjs`、`launcher/web/server/thcrap-*.mjs` |
| 资源代次与导入格式 | `contracts/src/`、Web package store |

## 准备与验证

站点组装使用显式 JSON 输入，入口为 `launcher/web/scripts/assemble-site.mjs --input=文件`。其中 `Games`、`RuntimeRelease`、`FeatureConfig`、`FontFile`、`VanillaFontFile`、`GameDirectories`、`Music` 和 `OutputDirectory` 必须对应实际资源。路径应由调用方明确传入；发布入口会按输入文件所在目录解析其正式路径字段。

音乐准备和 DATA 准备已分开。TH09/TH10/TH11/TH15 的原始归档可在不请求 OGG 时直接作为 DATA 输入；请求 OGG 则仍需真实 BGM 或声明的已准备目录。不要制作空音乐文件或伪造基准摘要使流程通过。

`midi` 是打包格式选择之一；对不支持 MIDI 的作品不会制造 MIDI 能力。只使用这项格式进行 TH15 DATA 测试时，其实际可用音乐是 `none`。

自托管的 `games.<game>.languages` 可显式选 `ja`、`lang_zh-hans`、`lang_en`；请求非日文语言必须有 Runtime 能力和对应语言资源。语言包格式适配不等于完整上游 thcrap 兼容。

## 字体与工具

通用入口不再默认为 Windows 的字体路径。Unicode 字体是明确输入；独立自托管包放在 `shared/unifont.otf` 并携带许可。Windows 自托管适配可探测系统日文字体，其他环境可提供 `shared/japanese-font.otf`，没有时使用明确的 Unicode 回退。

thtk 用于需要它的语言转换，不是所有日文站点的前置条件。Windows 按固定上游版本与摘要下载；其他平台使用已安装的工具。Python 环境按当前 requirements 准备；其范围版本和传递依赖尚未形成完整跨平台锁，列入审计改进项。
