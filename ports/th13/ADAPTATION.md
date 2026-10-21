# th13 适配验收记录

按仓库适配流程登记验收结果；每轮验收引用 `artifacts/validation/source-builds/` 中的构建记录。

## 构建模式

| 模式 | 命令 | 状态 |
|---|---|---|
| 网页运行时（sdl3） | `node portable/build.mjs` | 已通（UI 基座：标题/对局/暂停全链路，55 编译单元） |
| WASI 对照 harness | `node portable/build-harness.mjs` | 已通（全 game 层 wasm32-wasip1 同源编译） |

## 运行验收记录

- 2026-10-21 · 初始化轮 · 统一容器（`ports/th13/container`，podman/CI 共用）
  - 网页运行时：SDL3 主循环在浏览器安装运行（`web/sdl-runtime/th13.html`）。
  - RNG 行为闭环：Unicorn oracle 直调原版 `0x47BBA2`（CRT `_rand()`）得 MSVC srand(1) 标准序列；WASI harness 同种子输出逐值一致（`41 18467 6334 26500 19169 15724 11478 29358`）。
  - 单元测试：`node --test tests/*.test.mjs` 2/2 通过。
- 2026-10-21 · 第 1 轮 · ECL 虚拟机（行为规格 `docs/plans/ecl-vm.md`，命名对齐 th15 五模块）
  - 模块：`web/cpp/game/{Types,Interpolation,EclResource,EclContext,EclThreads,EclFile}` + `EclInterpreter.cpp`（基础指令 0–93 内联，>93 经 EclCommands 回调）；Rng 补 `GameRandom`（原版 0x472c90 游戏侧 16 位 LCG，反汇编逐指令还原）。
  - 取参语义勘误：容器内 capstone 直读 0x478e60/0x478ee0 反汇编，整型/浮点均 ≥0 局部、[-100,-1] 表达式栈（×8 步长，-1 为栈顶）、<-100 全局（浮点判定常量 -100.0/-1.0/8.0 在 0x4ae900/0x4ae908/0x4ae910）；规格 §1.3 整型行与此相反，以代码为准。
  - 单元测试：容器内 g++ 三套件 18/18 通过（`tests/ecl/run.sh`，真实 st01.ecl 字节固定用例 + 全 59 子程序冒烟）；`node --test tests/*.test.mjs` 3/3 通过（含 C++ 套件包装）；WASI harness 构建通过。
- 2026-10-21 · 第 2 轮 · UI 基座（行为规格 `docs/plans/ui-base.md`，模式照 th15：ANM 句柄驱动、Frame/Draw 分离）
  - game 层：`FrameScheduler/MenuCursor/GameInput/GameKeyboard`（照抄）、`Timer/Interpolation` 扩展（ANM 通道族）、`TitleScene`（主菜单→难度→角色→StartGame，Replay/PlayerData/MusicRoom 禁用待分期）、`GameHud` 三件套（残机 30–37/灵击 38–45 句柄、score/hi-score 逼近、灵力 x.xx/4.00）、`RunPause/PauseFrame/PauseMenu`（继续/重开/回标题，GameOver 禁用继续）、`RunScene`（HUD 宿主 + Esc/失焦暂停请求）、`AsciiText/AsciiFrame`（文本单通道）。
  - sdl 层：`Application/ApplicationState`（destination 驱动 + 装载挡板 + 16 槽 projection）、`GraphicsDevice`（ZunGraphics→portable Renderer，离屏捕获/高刷插值延后）、`SceneDisplay`、`FontDevice/AudioDevice/FileStore` 桩（接口先立）、`BrowserRuntime`（EM 导出 + RAF + FrameCadence，ApplicationHost 删除）。
  - portable 共享件：`portable/sdl/{Renderer,GraphicsState,RenderCommands,LegacyGraphics,Shaders,FrameCadence,PresentationCadence,AssetPixelFormat,ResampleShader,StartupBranding,ImguiRenderer.inc}` + `portable/input/KeyboardMap.inc`（双 Shift 归一 vk16）。
  - 脚本号登记：主菜单根=title script0（dump 20 子脚本）、提示=script3、难度/角色根=script1/2（结构推断，待 supervisor 锚定回填）；暂停菜单脚本待锚定（`PauseMenu.hpp` kPauseMenuScript）；HUD 残机/灵击脚本号已对 dump 验证（interrupt 2/3/7+n，碎片阈值 5）。
  - 单元测试：容器内 g++ 七套件 34/34 通过（`tests/ui/run.sh`：输入掩码与 repeat 参数、调度器/光标、ANM 句柄合成资源、HUD 布局与格式、标题全链路、暂停覆盖层、对局场景）；`node --test tests/*.test.mjs` 4/4 通过；WASI harness 全 game 层编译通过且 RNG 序列不变。
  - 与 anm-pipeline 工作流协同：ANM 核（AnmVm/AnmManager/AnmRegistry/AnmResource/AnmVariables/GameRandom）以其 Ghidra 核对版为准，UI 侧仅消费公共面并补齐 `AnmOverlayRenderer/AnmStrip` 桥接件。
- 2026-10-21 · 第 1-5 轮 · stage1 对拍收官（行为规格 `docs/plans/event-contract.md`）
  - 结论：3600 帧 10187 事件（spawn 84 / move 7611 / bullet 2492）与 unicorn oracle
    逐字节一致（`python3 -m oracle.stage1_compare` → `RESULT first_divergence_frame=none
    window=3600 events=10187`，exit 0）；oracle 基准重跑复现。
  - 五轮修复的主锚点：敌机链节拍（0x80000 自门/处理前取 next/构造趟后挂链/死亡摘链）、
    建敌镜像位（0x41fcc0 param2[6]）、坐标量化 floor（0x497690）、弹 z 恒 0.1
    （0x40e840 尾部覆写 0x3dcccccd）、弹角求值序（0x40e950-0x40e988 的 f32 中转落点）、
    enmKillAll 的 flags 门控销毁与清场 RNG 抽取（0x41ac70/0x417b10）、-9909 取
    trace_id、moveCircleTime 参数槽位（5 参）与插值角维恒零（0x41c627）、
    位置插值增量经 aux 通道（0x419450:71-92）。
  - 比对工具链：`oracle/stage1_compare.py`（--ulp 选项值解析修复）、
    `stage1_oracle.py` 的 c_int_arg 整型直通。
  - 单元测试：ecl 6/6、ui 3/3、anm 7/7、node 4/4 全绿；网页与 WASI 双构建通过。
- 2026-10-21 · 第 6 轮 · 录像解码与回放注入（行为规格 `docs/plans/replay.md`）
  - 模块：`web/cpp/game/ReplayFile`（"t13r" 头、两趟交织反转 XOR（0x7D/0x3A/0x100 +
    0x5C/0xE1/0x400，初始密钥经 AL 传参自反汇编锚定）、ZUN LZ77 位流（13 位地址+4 位长、
    环 0x2000 游标起 1）、0x74 顶层头 + 0xC4 stage 块链、USER 段）与 `ReplayInputCursor`
    （6 字节/帧 = 态/按下沿/释放沿三 u16，事件流每 30 帧 1 慢速率字节）。
  - 真机录像全量验证：`games/replays/th13_udSr01.rpy`（Kaz/Sanae/Normal/全通 24898353）
    解压 637666 字节精确命中声明尺寸，6 关 105476 帧沿差分零错；USER 段摘要
    （Rank Normal/Stage All Clear/Score 24898353）与顶层头字段互证。
  - harness 注入：`tools/run-harness.mjs replay` 逐帧 `GameInput::update` →
    `StageBattle::step`，一阶段 11064 帧全量注入跑通，Boss 出现帧 2380 实测登记。
  - 单元测试：replay 套件 8/8（XOR 双向、LZ77 字面量/重叠拷贝、合成 rpy 全链路往返、
    真机头/stage 链、首帧/Boss 前关键帧 2379/末帧、沿一致性、游标节拍）；
    `node --test tests/*.test.mjs` 5/5；网页与 WASI 双构建通过。
- 2026-10-22 · 第 7 轮 · 弹幕/自机弹渲染差距修复（弹型域数据驱动）
  - .sht 弹型域接入（StageBattle）：装载器语义取自原版 0x443e40 直读——+2 u16 块数、
    +0xc8 u32 指针表（+0xf4 基准）、记录步长 0x38、首字节 i8<0 终止；11 块 =
    5 能量档非聚焦（0-4）+ 5 档聚焦（5-9）+ 特殊块 10（类别 2，未接入）。pl00a.sht
    实测：主弹（类别 3）±10/−8 出膛、pl00.anm 脚本 14/15/16、附属弹（类别 8 =
    Option 卫星锚定）分期。射击按当前能量档 + 焦点态选块取主弹列；每轮总伤恒为
    3×kShotDamage 按列均摊（oracle 对拍孪生口径不变）。能量经济（kKillPower）仍为
    定标值——块档位随 power 偏高推进，原版道具拾取经济接入前为已知偏差。
  - 敌弹槽位复用换色修复（StagePresentation）：弹色变化即重绑 VM 并重放出生中断 2
    （旧实现同脚本复用槽位沿用前任弹色精灵，Boss 段实测 type5 弹显示 color2 精灵）。
  - 自机弹脚本化（StagePresentation）：.sht 记录的脚本号绑定 plXX.anm 弹型脚本
    （旋转模式 1 随弹向；焦点块含 ±5 副列），无 .sht 会话回退精灵直贴。
  - 取帧/诊断链路：shot-frames 报告后置（evaluate 间隔会令 WebGL 后备缓冲合成清空
    → 黑帧）+ 仅前向泵；th13-debug/th13_debug_* 增表现层 VM 清单、直绘过滤与
    renderer 探针导出；资产清单补 st01.std/st01wl.anm（STD 舞台链路装载依赖）。
  - 验收：`python3 -m oracle.stage1_compare` 重跑 3600 帧 10187 事件逐字段一致
    （first_divergence_frame=none）；ecl 6/6、anm 4/4、ui 3/3、replay 8/8、node 5/5
    全绿；回放 2500 帧注入跑通；th13-shot 取帧确定性自检通过（1242/2381 帧自机弹
    两列 ±10px 与原版一致、敌弹精灵随色匹配）。

- 2026-10-22 · 第 8 轮 · STD 舞台背景（背景/暗化/镜头类差距，全数据驱动）
  - 模块：`StageResource`（st01.std 解析：3 对象/24 图元/40 实例/st01wl.anm，
    头与字段布局经独立字节复核）、`StageScript`（相机 VM：ins_1=跳转(指令偏移,
    新时刻)——st01 在 t=1568 跳回 t=800 段构成 768 帧滚动环，同组换景指令为
    编辑残留不可达；指令面 1-9/12/16 为 th01-07 dump 全集，st02 的 ins_14 命中
    显式报错）、`StageScenery`（图元 VM 池 + 实例绘制：平移=图元+实例位置、
    缩放=STD 尺寸/精灵尺寸、雾 mode 8/24、深度写 flags 0x2000 取反、16 顶点
    包围盒投影裁剪）、`stage_camera`（SceneMatrices：视线 y 基镜像配后端 NDC
    y 翻转——golden f1242 鸟居（相机上方）落画面上部实证）。
  - 接入：RunScene 两组 Draw 回调（背景组层 0-7@3/前景组 8-11@4）＝全屏黑
    清屏→雾色清屏→实例绘制→关雾恢复全屏 2D 相机；bank 7=st01wl.anm。原版
    3D 取景为全屏（golden 左边带随场景明暗 13→73 变化、右边带/上下 16 行
    恒定=框架覆盖）——对局帧自此有清屏与背景，不再残留标题画面。
  - Boss 登场变暗：boss 槽存活（事件流实测 2381=MBoss spawn+512 setBoss 同帧）
    → 游戏区黑幕 30 帧淡入 0.6α；golden 实证 2381 未暗/2450 全暗≈0.35×/
    边带全程不暗（仅覆盖 128,16–512,464）。Boss 段 5494+ 无 512 事件不受
    影响（符卡背景另行分期）。
  - 对拍（th13-shot 全 74 golden 帧 + 确定性自检通过）：f61/121 场地 p50=0
    与原版一致（开场黑雾暗场成立；golden 均值差为开局文字/HUD 内容）；f1242
    夜樱走廊构图一致（视觉核对：鸟居/台阶/灯柱结构对齐，无镜像/错位）；
    f2381 顶带 (37,15,30)≈原版 (29,14,30)；f2450 变暗后 (21,9,15)≈(23,11,29)。
    已知残差：f0 蓝Flash 为速通视频转场；变暗色调原版留藏蓝底（纯黑乘 vs
    色幕）与边带亮度（front.anm 框架未接入）待后续轮。
  - 验收：anm 4/4（新增 STD 套件：golden 逐字段+运行时闭环+非法输入）、
    ecl 4/4、replay 1/1、ui 7/7、node 5/5 全绿；网页与 WASI 双构建通过。
