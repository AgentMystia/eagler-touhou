# 自托管指南

本指南面向独立自托管包，也适用于源码中的 Host 工具。它把已有 Runtime、运营者提供的原版资源和配置组装成可验证的静态站点。

## 准备输入

独立包使用 Node.js 22 或更新版本和 Python 3；源码开发使用[项目环境要求](https://github.com/YomotsuHisami/eagler-touhou/blob/main/docs/DEVELOPMENT.md)。构建会安装锁定的 npm 依赖，并在 `.cache/python/` 建立 Python 环境。首次运行需要取得这些依赖。

| 输入 | 位置及用途 |
| --- | --- |
| Runtime Release | `runtime-release/`，决定此次构建的游戏集合和可用能力 |
| 原版资源 | `games/<game>/`，只要求本次 Runtime Release 里的游戏 |
| Unicode 字体 | `shared/unifont.otf` |
| 日文字体 | `shared/japanese-font.otf`；按实际资源准备要求提供 |
| 可选配置 | `eagler-touhou.config.json` |
| 可选美术覆盖 | `artwork/`，使用准备工具支持的文件名 |

Runtime Release 不包含原版 DATA。请使用自己有权使用的资源；不要把资源、存档或私有部署配置提交到源码仓库。各作文件要求由 `lib/content-definition.mjs` 定义，`host:doctor` 会报告具体缺项。

源码调用可以用 `--root=HOST_DIRECTORY` 指向独立输入目录，不必把资源放进源码树。

## 选择语言和练习功能

不填写游戏配置时，Host 根据 Runtime Release 的能力选择语言和练习支持。只需日文时可以显式选择，避免无用途地准备其他语言：

```json
{
  "schema": "eagler-touhou/host-config/1",
  "games": {
    "th15": {
      "languages": ["ja"],
      "thprac": true
    }
  }
}
```

这里只能配置本次 Release 中的游戏及其已声明能力。若 Runtime 不支持练习或翻译，配置会报错，不会悄悄忽略。中文、英文的语言 ID 是 `lang_zh-hans`、`lang_en`。

## 构建与检查

在独立包根目录运行：

```text
npm run host:doctor
npm run host:build
npm run verify:server -- dist/site
```

`npm run host` 构建后启动本地 HTTP 服务并尝试打开浏览器。`--no-open` 关闭自动打开；`--bind` 和 `--port` 配置本地服务。公共部署应使用运营者自己的 HTTPS 服务。

默认音乐选择为 `midi,ogg`，每作只生成其支持的部分。`--music=midi` 可避免 OGG 转码；对不支持 MIDI 的作品，这意味着无背景音乐。DATA 准备与音乐准备相互独立，不以启用 OGG 作为构建 DATA 的前提。

```text
npm run host:build -- --music=midi
npm run host:build -- --root=HOST_DIRECTORY --python=PYTHON_EXECUTABLE
```

翻译准备需要 thtk：Windows 下载并校验固定版本；其他平台需要 `thdat`、`thmsg` 在 PATH。只构建日文时不会无条件要求翻译工具。详细说明见[参考](SELF_HOSTING_REFERENCE.md)。

## 导入模式与外部资源

`npm run import` 生成 `dist/import-site/` 和每个所选游戏的 `dist/import/<game>.zip`。导入站点保留启动器和 Runtime，原版资源由用户选择本地包导入。

`external` 从同一个已验证 Hosted 站点派生，网页服务器把游戏及共享资源请求转向配套资源源站。请按[外部资源模式](EXTERNAL_RESOURCE_MODE.md)配置，两边不能混用不同代次。

## 部署后验证

把验证通过的整个站点作为一个版本交付，再切换服务器指向。随后运行：

```text
npm run verify:deployed -- https://example.com/
```

再用目标浏览器完成一次资源安装、进入游戏和存档重载。HTTP 检查不会替浏览器执行 CORS，也不会证明游戏运行正确。

自托管包制作见[项目发布流程](https://github.com/YomotsuHisami/eagler-touhou/blob/main/docs/playbooks/release.md)。上述构建命令不执行上传、GitHub 发布或生产切换。
