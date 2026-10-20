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
