# 构建与验证产物

源码、游戏资源和生成产物各有自己的身份。产物目录名说明用途，不说明某个游戏已经通过验收。

## 输入与输出

| 产物 | 入口与位置 |
| --- | --- |
| 游戏源码 | 根目录子模块和明确指定的普通/多人源码 |
| 游戏编译 | TH06/TH07 的 `build/web/<variant>/`；目录 Runtime 默认根 `dist/runtime/<game>/<variant>/` |
| Runtime Release | `launcher/web/scripts/package-runtime-release.mjs` 写入新的显式目录 |
| Web 前端 | `npm run build` 生成 React 与配套资源，目录由构建配置选择 |
| 自托管包 | `build-self-host-bundle.mjs` 生成独立目录和 ZIP |
| 正式候选 | `release.mjs --input=... --output=...`，包含清单、附件和校验 |
| 验证证据 | 调用者明确拥有的目录，记录输入、版本、范围和原始结果 |

正式构建读取显式参数、声明和已验证的 Runtime Release，不扫描旧 `dist`、实验目录、截图目录或历史缓存来挑选产物。新输出不能覆盖未经管理的目录，失败时保留上一份有效产物。具体构建见[教程](playbooks/runtime-build.md)，交付见[发布](playbooks/release.md)。

## 内容和闭合清单

Runtime Release 包含游戏代码和声明的运行资源，不包含原版 DATA、BGM、个人存档或私人 Replay 样本。目录 Runtime 的文件集合、尺寸和摘要必须与 `runtime-files.json` 一致。

站点由同一份 Runtime、资源和配置产生 `site-manifest.json`、`release-catalog.json`、Package Descriptor 和 App Shell。site manifest 描述此次 Runtime/资源，release catalog 指向可安装 Package；不增加另一份 `games.json`。

Hosted、External、Import 是资源模式；development、validation、release 是构建用途，不能混用。Import 站点仍有 Launcher 与 Runtime，原版内容由用户安装。External 的网页与资源源站由同一代次派生。

## 保留什么

- 可重复生成的编译缓存和临时站点留在忽略的本机目录，不进入公开源码。
- 原版数据、工具链、字体私有输入和密钥不随源码或测试证据发布。
- 必须保留的失败现场、前后源码和验证报告放在明确的外部证据目录。
- 审阅用候选记录源码提交、子依赖、工作区修改、Runtime 文件身份和实际检查结果。

路径、目录名和输出摘要不能证明游戏逻辑正确。某次验证记录只覆盖当时源码、输入与环境，后续变更按受影响的行为重新验证。
