# 开发指南

## 环境

源码工作区使用 `Node.js ^24.15.0 || >=26`，与根 `package.json` 一致。使用根 `package-lock.json` 安装三个 npm workspace 的依赖。独立自托管包有自己的 package/lock，运行要求见包内说明。

Web 构建需要 Node；仓库 Python 检查需要 `launcher/web/requirements-ci.txt`。游戏构建还需要对应 Emscripten、CMake/Ninja 或 WASI 工具链。环境探测与构建输入以 `npm run doctor` 和具体构建报错为准。

```text
npm ci --ignore-scripts
python -m pip install -r launcher/web/requirements-ci.txt
npm run doctor
npm run dependencies -- status
```

非 Windows 系统若只有 `python3`，把示例中的 `python` 换成 `python3`。工程入口也可通过 `--python` 或 `PYTHON` 指定解释器。

## 初始化依赖

```text
npm run dependencies -- init th06 th07
npm run dependencies -- init eagler-common
git submodule status --recursive
```

该入口按 Git 子模块提交初始化依赖。不要用随意拷贝的兄弟目录代替提交锁定。切换主仓库版本后应检查子模块状态；已有子模块修改须先妥善保留，不能直接重置。

各游戏及其递归依赖保留独立历史。修改子模块后先在子模块提交，再由主仓库记录新的 gitlink。共享更改时，远端必须能获取这些子提交；主仓库一个提交不能替代子仓库发布。

## 日常命令

| 命令 | 用途 |
| --- | --- |
| `npm run dev` | 启动当前 Web 开发服务 |
| `npm run build` | 编译浏览器服务和 React 静态入口 |
| `npm run check` | 项目检查，包含 Web 核心、类型、UI 与服务等；耗时较长 |
| `npm run check:workspace` | 加上 common、TH06、TH07 的原生 CMake/CTest |
| `npm run test:services` | 独立联机服务行为测试 |
| `npm run format:check` | 维护源代码格式，不冻结业务文本或文件哈希 |

按改动选择较小的验证集合见[验证 playbook](playbooks/verification.md)。产物组包含真实构建，按测试入口串行执行。

## 本地配置

`config/local.json` 被忽略，可保存 `emsdk`、`python`、`cmake`、`ninja`、`fontRoot`、`imguiRoot`、`runtimeExtension` 和按游戏区分的 `assets`。显式命令参数优先。不要提交个人绝对路径、存档、原版数据或部署密钥。

`config/workspace.json` 描述正式目录，不承担个人机器配置。TH20、完整 thprac 扩展、工具链目录是可选外部输入，没有相应目录时不代表已安装或已支持。

## 构建与交付

参见 [Runtime 构建](playbooks/runtime-build.md)、[资源准备](playbooks/resources.md)和[发布](playbooks/release.md)。构建入口不创建 worktree；需要特殊多人分支时，使用已经准备好的显式源码目录。

提交描述应说明真实改变。公开更新说明、推送、发布和部署按项目的明确授权执行，不由构建成功自动触发。
