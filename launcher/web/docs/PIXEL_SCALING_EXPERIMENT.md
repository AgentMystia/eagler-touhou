# 浏览器像素缩放实验

本实验只控制最终 Canvas 的 CSS 显示，不改变画布缓冲区、游戏纹理过滤、模拟或存档。

- 原始：恢复游戏原有 CSS，作为基准。
- 清晰像素：仅开启 `image-rendering: pixelated`，保持原有显示尺寸。
- 整数倍：在当前画面区域内选取适合屏幕 DPR 的最大整数倍率，居中显示。空间不足时退回清晰像素。
- 关闭并恢复：移除面板并恢复修改过的行内样式。

使用浏览器默认缩放，关闭游戏手势放大后比较。整数倍率不等于在所有嵌套变换、浏览器缩放环境下都保证物理像素对齐。若清晰像素模式下整张画面变清晰，但个别精灵仍模糊，应另查游戏内部纹理采样。

## 本地运行

已有本地游戏站点时，在 `launcher/web` 运行 `node scripts/preview-pixel-scaling.mjs`。默认从本机 8080 端口代理到 `http://127.0.0.1:8086`。用环境变量 `PIXEL_UPSTREAM_PORT` 和 `PIXEL_PREVIEW_PORT` 调整端口。仅监听本机，不用于部署。实验入口禁用离线 worker，避免它替换临时页面。该端口拥有独立的浏览器存储，可能需要重新导入游戏资源。

当前实验也可随 Launcher 构建，通过网址参数 `?pixelExperiment=1` 开启。参数关闭时不加载实验脚本。支持同源 iframe 中的游戏画布。

若开发站点声明的资源路径与当前文件布局不同，可用 `PIXEL_MOUNTS` 指定 JSON 对象，将 URL 前缀映射到本地目录，例如 `{"/dist/runtime":"D:/workspace/eagler/eagler-touhou/dist/runtime"}`。仅向本机提供显式指定的目录，不上传资源。

依次对标题文字、HUD、静止精灵比较三种模式，并调整窗口大小。若“原始→清晰像素”明显改善，说明最终 Canvas 缩放至少是原因之一。本实验尚不代表各作内部纹理采样或真机移动端验证。

浏览器验证：`node tests/test-pixel-scaling-experiment.mjs`，使用已安装的 `puppeteer-core` 和 Edge，可通过 `PIXEL_PUPPETEER`、`PIXEL_BROWSER` 指定驱动模块和浏览器。覆盖同源 iframe、DPR 1 / 1.25 / 2、整数倍率、切换及关闭恢复。

实验分支从 `launcher-refactor` 的 `b6aa0ae` 建立，不包含原工作目录的未提交改动。临时预览可代理现有构建进行对比，不宣称它来自实验分支重建。

2026-10-15 本地预览已使用 `refactor-core-c5` Launcher 产物和现有 TH10 Runtime 启动游戏，状态到达“TH10 ogg 已就绪”。原始画布为 640×480，在 1400×1050、DPR 1 的浏览器窗口中显示为 1390×1042.5，倍率约 2.172。资源映射仅用于该本地预览。游戏卡片插图和 PWA 图标在本地缺失，不影响此画面缩放实验。
