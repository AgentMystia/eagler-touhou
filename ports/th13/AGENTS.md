# th13 局部入口：先接手，再施工

本作目标是原版 v1.00c 的行为、真实渲染与网页最终游戏画布还原。即使你是主 Agent，也不得自行豁免验收。

## 工作区连接

本开发工作区的外层根是本目录向上三级：`../../../`（宿主 `/home/mystia/项目/th13`，容器 `/work`）。先读外层 `AGENTS.md`；其红线、事实报告和验收要求优先于 monorepo 的宣传文风。

宿主执行：

```sh
make -C /home/mystia/项目/th13 exec CMD='python3 scripts/verify.py doctor'
make -C /home/mystia/项目/th13 exec CMD='python3 scripts/verify.py resume'
```

已经位于规定容器内时：

```sh
python3 /work/scripts/verify.py doctor
python3 /work/scripts/verify.py resume
```

随后只按返回任务卡执行 `task show/start`、`run --tier fast/full`、`task finish`、`handoff`。运行信息和完成资格不从旧 HANDOFF 或模型记忆恢复。

若这是独立 monorepo checkout，外层 `scripts/verify.py`、策略或原版资格证据不存在：**报告 BLOCKED，缺失项列清楚；不能复制一套临时 runner、猜测 golden 或宣布测试即还原通过。** 产品本地单测仍可作为明确标注的诊断，但不能代替外层资格认证。

## 本作固定边界

- 所有构建/测试/分析经指定容器；不在宿主安装依赖。
- 原版资产、Replay、反编译 dump、原版帧和捕获产物不得加入本仓库；放外层 ignored `games/`、`artifacts/`。
- 不接触凭证；不清理用户已有修改；用户未要求时不提交或推送。
- `portable/sdl` 共享后端与 th15 零静默漂移。本作差异在配置/调用点，分叉需独立授权与原版证据。
- 生产提交是唯一绘制真值。trace 只能观察同一状态/几何，禁止只写捕获不画网页；legacy DrawCapture/harness 不认证网页。
- 捕获完整、历史不退化、行为一致、渲染语义一致、实际 framebuffer 一致分别报告。
- 原版假设备清单相同不证明真实 GPU 输出相同；缺独立原版输出环境时最终画面认证 BLOCKED。
- 每批整体 glBufferData，重复展示不改游戏状态；句柄生命周期、资源内容版本和提交顺序必须保留。
- 每任务最多三轮无机制进展后停止磨帧，按任务规定保留最小复现和阻塞证据。

接口与契约在本作 `docs/`；任务图与运行证据在外层 `verification/` 和 `artifacts/verification/`。不要建立第二套完成状态。
