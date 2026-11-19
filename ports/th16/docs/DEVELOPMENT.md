# th16 开发指南

## 原版与校验

| 文件 | 字节 | SHA-256 |
| --- | --- | --- |
| `th16.exe`（v1.00a，PE 时间戳 2017-07-31T20:47:09Z，入口 0x0047537b） | 683520 | `c11776019f083978e66027e7394dafb1fb9543afca986f28049a49417e341929` |
| `th16.dat`（THA1） | 77693268 | `c84e04181c1c08c80fb0eaba641cec9eb27244ac372749314bd886e3e5a4b64f` |
| `thbgm.dat` | 388876780 | `2904df926a97cd04c7c9d96abdcea802fab76b5432bc7359d4c3f4d11e055e4d` |

放在工作区根 `games/th16/original/`（ignored）。`web/target.json` 登记同一组身份，`analyze.mjs` 导出前校验。

## 工具链（工作区根 `toolchains/`，ignored）

| 工具 | 版本 | 用途 |
| --- | --- | --- |
| emsdk | 6.0.9 | `portable/build.mjs`，`--use-port=sdl3`，WebGL2 ↔ GLES 3.0 |
| wasi-sdk | 34 | `portable/build-harness.mjs`，无头 harness（wasm32-wasip1） |
| JDK + Ghidra | 21 / 12.1.3 | `web/scripts/cpp/analyze.mjs` headless 导出 |
| thtk | 源码构建 | `thdat -x16` 解包、`thanm -l16/-x16`、`thecl -d16`、`thstd`、`thmsg` 对照 |
| python venv | unicorn 2.1.4、capstone、pefile、pillow、numpy | `tools/oracle/*.py` |

## 构建

```bash
EMSDK=../../toolchains/emsdk node portable/build.mjs            # web/artifacts/sdl3/th16-sdl.{mjs,wasm} + build.json
WASI_SDK_PATH=../../toolchains/wasi-sdk node portable/build-harness.mjs   # web/artifacts/wasi/th16-harness.wasm
node tools/run-harness.mjs anm games/th16/original/th16.dat title.anm      # 与 tests/build/harness 同参数
```

两条构建共用 `tools/build/source-object.mjs` 的逐 TU 对象缓存；任一头文件、flags、编译器身份变化即全部失效。失败的构建会删除产物与 manifest。

## 浮点语义

`th16.exe` 以 SSE2 标量单精度编译：4289 条 SSE 标量指令对 428 条 x87；RNG 的浮点变体是 `CVTPD2PS` 后 `MULSS/SUBSS/DIVSS`。因此 C++ 侧全部使用 `float` 并禁用 FMA 融合（`-ffp-contract=off`），对 Unicorn 直跑原版函数的结果位级一致（`tests/golden/rng.json`）。仅 43 个游戏函数使用 x87 算术（34 处超越函数），这些在还原时逐个对照 `.asm`。

## Oracle 与测试

```bash
bash tests/run.sh                                   # 本机 harness + node --test
toolchains/venv/bin/python tools/oracle/rng-oracle.py          # 重生成 tests/golden/rng.json（Unicorn 直跑 0x402b70/0x402be0）
toolchains/venv/bin/python tools/oracle/thtx-compare.py tests/build/harness games/th16/original/th16.dat \
    artifacts/th16/dumps/anm artifacts/th16/dumps/textures title.anm sig.anm      # THTX 解码对 thanm -x16 逐像素
```

`tests/archive.test.mjs` 在 `artifacts/th16/dat`（thdat 解包）与 `artifacts/th16/dumps/anm`（thanm -l16）存在时对照 215 个档案条目的字节与 ANM 结构；缺原版时跳过并说明。

### ANM VM oracle

```bash
# Unicorn 直跑原版：构造 AnmManager(0x46a3a0) → ascii.anm 进槽 2 → 装载目标 ANM → 每个脚本 spawn(0x406380) + 60 帧 tick(0x46e340)
toolchains/venv/bin/python ports/th16/tools/oracle/anm-oracle.py games/th16/original/th16.exe title.anm \
    artifacts/th16/dat/title.anm all 60 artifacts/th16/oracle/title.json --extra 2:ascii.anm:artifacts/th16/dat/ascii.anm
node tools/oracle/anm-golden.mjs ../../artifacts/th16/oracle/*.json      # 压成 tests/golden/anm-vm.json（每脚本 sha256）
tests/build/harness anm-run ../../games/th16/original/th16.dat title.anm all 60   # 本实现的同格式快照
tests/build/harness anm-bench ../../games/th16/original/th16.dat title.anm 0 3600 # 只推进（性能口径）
```

oracle 只桩掉堆分配、档案读取、D3D 纹理（按 THTX 头尺寸回报）、临界区与 Sleep，其余全为原版代码；装载完成后做内存检查点，每个脚本从同一状态开始。快照为 VM 结构体 0x568 字节纯数据区（其后为链表指针）加 36 条 layer 绘制链；`tests/anm-vm.test.mjs` 对 54 个 ANM 文件 1887 个脚本逐一比对摘要，失败时若本机有完整 oracle JSON 则报出首个差异字段。`effect.anm` 155–196 需 EffectManager（ins_508），待该子系统接入后加入 golden。

`tools/run-harness.mjs` 对 stdout/stderr 的 `fd_write` 用 JS 同步写出：Node 22 自带 uvwasi 在输出到文件或 `/dev/null` 时会段错误（同一 wasm 在 wasmtime 下正常）。

### 绘制调用流 oracle

渲染侧的对拍单位是"每次 Clear / 绘制时生效的设备状态 + 顶点字节"，不比较冗余的 SetRenderState 调用次数：

```bash
# 原版：--draw 让每帧按 0x45a8a0 顺序跑绘制链，D3D 调用由 tools/oracle/d3d-recorder.py 录成 JSON Lines
toolchains/venv/bin/python ports/th16/tools/oracle/game-oracle.py games/th16/original/th16.exe artifacts/th16/dat 820 \
    /tmp/st1.json --stage 1 --shot 1 --season 2 --difficulty 1 --lives 8 --draw --draw-out /tmp/st1.draw.jsonl --draw-from 800
# 移植版：game-draw 与 game-run 同参，末尾加 <out.jsonl> [from_frame]，用 tests/harness/RecordingDevice.hpp 录同口径数据
tests/build/harness game-draw ../../games/th16/original/th16.dat 1 1 2 820 "" "" "" 1 -1 8 0 0 /tmp/st1.port.jsonl 800
# 对拍：目标 / 视口 / 清屏参数 / 图元 / 顶点格式 / 顶点字 / 渲染状态子集 / 世界格式的矩阵；退出码 1 表示有差异
python3 tools/oracle/draw-compare.py /tmp/st1.draw.jsonl /tmp/st1.port.jsonl --frames 800-820
```

定位差异时先看单侧的逐操作清单：`draw-compare.py … --dump oracle`（或 `--dump port`）按帧列出每个 Clear / 绘制的纹理槽、视口、
前四个顶点；oracle 侧每项带 `[0x地址]`，即 DrawPrimitiveUP 发生时正在运行的绘制链回调（0x4015a0 链游标处的钩子记录），
因为批量顶点是惰性出批，归因可能落到下一条链。移植版再配 `TH16_DUMP_LAYERS=<帧>` 把该帧各图层链上的 VM
（槽 / 脚本 / 精灵及其来源槽 / 渲染模式 / 位置 / 颜色 / 标志）打印到 stderr，逐 VM 对账。
尚未移植的子系统用 `--skip-textures 2`（AnmFile 槽号，2 = ascii.anm 文字）在两侧同时剔除其绘制，其余操作就能逐条对位；
不能按链地址剔除，因为链回调里的出批会把上一层的残批记在它名下。注意被剔除的子系统若切相机（如 AsciiManager 的
0x408ef0 切到场地相机再切回 UI 相机），紧随其后出批的那条绘制在移植版会记到不同的视口，这类差异要等该子系统移植后才消失。

回归守护在 `tests/draw.test.mjs`：原版流经 `draw-compare.py <oracle.jsonl> - --structure-golden tests/golden/draw-st1.json --frames a-b`
折叠成每帧一条 sha1（口径 = `structure_lines`：目标 / 视口 / 图元 / 格式 / 纹理 / 状态子集，**不含顶点与矩阵**——原版 x87 与
移植版 SSE 单精度在个别顶点差 1 ulp，位级摘要会误报），测试跑 `game-draw` 后按同一口径复算；原版 jsonl 本身（数十 MB）不入库。
顶点级对拍仍用上面的 `--eps` 命令手工做。场景里还有未移植子系统时加 `--only-textures 7`：摘要只含用该 AnmFile 槽贴图的绘制
（Clear 一并丢弃），槽号写进 golden 的 `texture_slots`，测试侧按同一过滤复算——`draw-st3-curve-laser.json` 等激光黄金就是这样
只守 bullet.anm（弹幕 + 激光），不被同场景尚未移植的 Boss 畸变光环 / 特效条带 / Boss 血环拖住。golden 的 `config.replay_slot`
（默认 −1）用于符卡练习槽直入。

`scene-boot` / `anm-draw` 也经 RecordingDevice 输出可读的 `quad` 行（屏幕四边形直接取顶点，世界四边形按 world·view·projection 投影并算线性顶点雾）。XYZRHW 顶点不经 TnL，原版从不关 TEXTURETRANSFORMFLAGS 而移植版对屏幕批显式关掉，draw-compare 对屏幕格式不比较该位。

## 页面壳

`web/sdl-runtime/th16.html` 把 `assets/th16.dat` 整档 fetch 进 MEMFS `/assets/th16.dat`，C++ `Archive` 按名解密/LZSS 解码——与原版读档路径同构，不预解包。`python3 web/sdl-runtime/serve.py` 提供 no-store 服务。观测面：`mod._th16_frame()`、`mod._th16_phase()`、`mod._th16_audio_status()`。

## 音频

`web/cpp/sdl/AudioDevice.*`：miniaudio 无设备混音 + SDL3 音频流（44100 Hz 立体声 f32，队列保持 2048..3072 帧）。音效声部按 `kSoundDefinitions` 绑定 th16.dat 内 `se_*.wav`；乐曲串流器首选与 th16.dat 同目录的 `thbgm.dat`（按 `thbgm.fmt` 条目直接读 PCM、循环点同原版），否则取 `<同目录>/music/<曲名>.ogg`（PCM 帧数须与 `thbgm.fmt` 一致，否则报错不播）。浏览器不进 388 MiB 的 `thbgm.dat`：把 18 首 `assets/music/<曲名>.ogg`（不入库，例如 `ffmpeg -f s16le -ar 44100 -ac 2 -i <按 fmt 偏移截出的 PCM> -c:a libvorbis -q:a 4`）放到 `web/sdl-runtime/assets/music/`，页面壳并行取进 MEMFS `/assets/music`，缺则该曲静音。音频打不开时逻辑（命令队列 / 音效队列）照常推进，只是无输出。

原版 BGM / SE 音量变量（DAT_004df49c / DAT_004df4a0）只在 Option 画面 0x44dc70 写入（开机到首次进 Option 为 0），端口开机即以配置值出声；0x470bb0 每次开曲重建 DirectSound 缓冲，端口在 open 时按配置音量重置。这些只影响宿主音量，不进任何对拍。

## 代码质量与可读性标准

见 [`docs/CODE_QUALITY.md`](./CODE_QUALITY.md)：命名规范（领域语言、禁止反编译偏移名）、文件与模块结构、函数控制流拆分、注释三要素以及精度/对拍/双处登记门禁。
