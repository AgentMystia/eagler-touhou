# 站点资源

## 游戏卡片与站点美术

`scripts/prepare-host-artwork.py` 从运营者提供的原版资源准备所选游戏卡片，输出 `assets/<game>-card.webp`。也可以使用工具支持的自定义图片覆盖。WebP 输入保留原字节，PNG/JPEG 转为 WebP；默认编码参数为 quality 55、method 6。

卡片、背景和其他原版衍生图片属于站点构建输入或产物，不因文件名出现在源码中就属于公开源码资源。原版图片不随无 DATA 的 Runtime Release 分发。

`favicon.ico` 来自 `font-touhou98` 的 U+262F 阴阳字形，组装到 `assets/favicon.ico`。`prepare-host-artwork.py` 生成 PWA 的 192、512、maskable 和 Apple 图标。运营者可按工具输入约定覆盖。

## 字体

站点界面使用 Yatra One 的拉丁字母/数字子集与 ChillRoundGothic 的 CJK 子集，GNU Unifont 作为缺字后备。`scripts/build-site-ui-fonts.mjs` 和 `build-site-unifont.mjs` 定义准备过程；字体目录保留相应许可。

游戏字体与站点字体职责不同。完整游戏字体由运营者提供；翻译准备成功不代表所有日文字形、符号和页面排版已验证。

`public/assets/fonts/touhou98.woff2` 用于字标。项目 README 的轮廓字标及预览图位于项目根 `docs/assets/`。

## 图标与署名

公告的 Bilibili、QQ、GitHub 图标来自 Font Awesome Brands；界面和触控中的 Material Symbols 使用本地 SVG 路径。Touhou Cloud 标识用于服务来源识别。房间图标来自 Phosphor，署名与许可见 [THIRD_PARTY.md](THIRD_PARTY.md) 及资源旁的许可文件。

房间头像目前使用文字占位；不要把占位显示描述为真实用户身份或新发布美术。公开资源清单由发布策略检查；增加 `public/assets/` 文件后，需要同时确认来源、许可和实际用途。

播放器的播放、暂停、直播、聊天及座位图标同样来自 Phosphor Icons，固定来源提交为 `2b75f3ad12b420c9504ef05df8d2564a28f8500e`。`public/assets/phosphor-ui-icons.json` 记录五个 SVG 的原始路径与 SHA256，`LICENSE-Phosphor.txt` 保留 MIT 原文；发布审计只允许这些明确登记的文件，不放宽其他资源规则。
