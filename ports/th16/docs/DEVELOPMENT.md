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

## 页面壳

`web/sdl-runtime/th16.html` 把 `assets/th16.dat` 整档 fetch 进 MEMFS `/assets/th16.dat`，C++ `Archive` 按名解密/LZSS 解码——与原版读档路径同构，不预解包。`python3 web/sdl-runtime/serve.py` 提供 no-store 服务。观测面：`mod._th16_frame()`、`mod._th16_phase()`、`mod._sdl_stats()`。
