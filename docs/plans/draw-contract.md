# 语义绘制清单契约（draw contract）v1

原版侧采集端 `oracle/render_oracle.py`（Unicorn + 假 D3D9 COM 设备）与移植侧
（同构导出器）之间的逐帧比对契约。范式对齐 `event-contract.md` /
`stage1_compare.py`：**JSONL、每帧一行、键序固定、浮点 `%.9g`（binary32 往返
无损）、末行 `RESULT first_divergence_frame=<n|none> window=<帧数> ops=<数>`**
是唯一裁决。

## 0. 驱动窗口（与事件流基线同窗）

| 窗口 | 帧号范围 | 行数 | 基线对照 |
|---|---|---|---|
| 无输入窗 | frame 0..3599 | 3600 | stage1-oracle.jsonl（10187 事件，末帧 3599） |
| 回放窗 | frame 0..10662 | 10663 | stage1-oracle-replay.jsonl（48406 事件，末帧 10662；th13_udSr01 stage0） |

frame 0 = 装载 + root spawn（含构造内首趟）。每帧序（对齐 0x45d2f0 帧序）：

```
0x418e00(EDI=ctl)     ECL 逻辑（事件流基线同拍）
0x46e510()            ANM 增量装载推进
0x46f170(EAX=mgr)     主池全部 AnmVm 步进 + 重建层链
0x46f240(EAX=mgr)     前景池步进 + 重建层链
BeginScene → 0x467960 清批 → 0x43b080(EDI=gamestate) fog 刷
→ 逐层 0x46f380(EAX=层, EDI=mgr)（主池 0..30 + 前景池 32..56）
→ 0x4679a0(ESI=mgr) 终批 → SetTexture(0,0) → EndScene → Present
```

前置装载（战斗初始化最小集，模拟 0x42b990/0x4145c0 序）：
- 0x4038a0(ESI=buf) 建 game_main（DAT_004c2160）
- 0x40d920() 建弹幕管理器（root 0x9752b8、bullet.anm mgr 槽 7 →
  root+0x9752b4；root 挂 DAT_004c2174）→ `game_main+0x19194` = bullet.anm file
- 0x46df40(EAX=8, ECX="effect.anm") → gamectx+0x10
- 0x418d30("st01.ecl")（ANIM 列表 → mgr 槽 10/11、ctl+0x88/0x8c）
- ECL 文件索引表 `ctl+0x80+idx*4`：0=bullet.anm、1=effect.anm、
  2..=st01 anim 列表（enemy.anm、st01enm.anm）

ECL no-op 跳表改写 = stage1 `NOOP_OPS` 剔除 ANM 指令族
（302/303/306/307/308/313/316/319 真实执行）；518→525 尾块保持。

## 1. 帧行格式

```json
{"frame":N,"viewport":[x,y,w,h,minz,maxz],"ops":[op,...]}
```

- `viewport`：本帧 SetViewport 的 6 float（%.9g）；无 SetViewport 的帧为
  `null`。层序即 `ops` 数组序（提交序）。

## 2. op 格式

### 2.1 quad（矩阵路径；IDirect3DDevice9::DrawPrimitive）

```json
{"seq":k,"op":"quad","prim":5,"anchor":a,"count":c,
 "tex":[槽,entry,"纹理名"],"layer":L,
 "blend":[src,dst,bop],"tfactor":u32,"fvf":F,
 "world":[16×%.9g],"texmat":[16×%.9g],
 "view":[16×%.9g],"proj":[16×%.9g]}
```

- `prim`：D3D 图元类型（TRIANGLESTRIP=5）。`anchor`：startVertex（静态 VB
  九宫格锚点组号 ×4，render-chain.md §2.1 第 10 步）。`count`：图元数（2）。
- `tex`：`[mgr 槽号, entry 序号, name1（cp932 解码，如 "enemy/enemy.png"）]`；
  `SetTexture(0, NULL)` 后的绘制为 `null`；未知纹理 `[-1,-1,"ptr:%08x"]`
  （**比对视为分歧**，不允许静默）。
- `layer`：提交时经 0x46f380 进入的层号（主池 0..30、前景池 32..56）。
- `blend`：`[SRCBLEND(19), DESTBLEND(20), BLENDOP(171)]`（§5.2 八模式表）。
- `tfactor`：SetRenderState(TEXTUREFACTOR=60) 的 ARGB u32。
- `fvf`：本绘制时刻设备 FVF（矩阵路径 0x102=XYZ|TEX1）。
- `world`/`texmat`：本次 DrawPrimitive 前最近的 SetTransform(D3DTS_WORLD
  =0x100 / D3DTS_TEXTURE0=0x10) 全 16 元（行主序，%.9g）。
- `view`/`proj`：同理 TS_VIEW=2 / TS_PROJ=3（帧内通常恒定）。

### 2.2 up（预变换顶点路径；DrawPrimitiveUP）

```json
{"seq":k,"op":"up","prim":P,"count":C,"stride":S,
 "tex":[槽,entry,"名"],"layer":L,"blend":[...],"tfactor":u32,"fvf":F,
 "verts":[[x,y,z,rhw,argb,u,v],...]}
```

- `verts`：C×顶点，每顶点按 stride 展开：FVF 0x144（stride 0x1c）为
  `[x,y,z,rhw,diffuse_u32,u,v]`；FVF 0x44（stride 0x14）为
  `[x,y,z,rhw,diffuse_u32,0,0]`。浮点 %.9g；`argb` 为 32 位色（u32 十进制）。
- 一次 DrawPrimitiveUP = 一个 op（0x4679a0 批提交的全部 6×count 三角形顶点
  平铺）。

## 3. 数值与比对规则

- 全部 float 以 `%.9g` 序列化（与 event-contract §5 相同，binary32 往返无损）；
  NaN 序列化为 `"nan"`，两侧按「任一 NaN == 任一 NaN」处理（cmp_float 语义）。
  【2026-10 对齐轮勘误】v1 基线里 world/texmat 的 NaN 并非「角度未初始化
  pattern 0xfff0bdc1」，而是 oracle 假设备三处缺陷的产物（GetLevelDesc 误以
  ECX 取 this、D3DSURFACE_DESC 多写一个幻影 Size 字段致 Width 错位到 0、
  D3DXCreateTexture 的 cdecl 栈参偏移整体错 8 字节使纹理登记宽高失真）→
  精灵记录 X 维 0/0=NaN。三缺陷修复后重导出的基线不再含此类 NaN。
- `draw_compare.py`：逐帧逐 op 逐字段；浮点按 **ULP 容差**（默认 4 ULP，
  `--ulp N` 可调；位级相同直接通过）；整数字段（prim/anchor/count/tex 槽/
  entry/layer/blend/tfactor/fvf/argb）精确相等。
- 顶点数组逐元素比对；矩阵逐元素比对。
- `tex` 名字段参与比对（纹理内容不比——动态纹理/后台缓冲拷贝按事件语义比，
  见 §4）。
- 分歧报告：首个不匹配的 (frame, seq, 字段, 两侧值)。末行
  `RESULT first_divergence_frame=<n|none> window=<帧数> ops=<两侧 op 总数较大者>`。

## 4. 已知边界（v1 不覆盖、如实标注）

1. **纹理内容不比**：像素（THTX/文本 GDI 上传/后台缓冲→@R 拷贝）不进清单；
   动态纹理在清单中就是普通 tex 引用（名字 + 槽稳定）。
2. **stage 侧固定层**：0x4032d0 的关卡 ctx 内嵌 5 AnmVm、HUD、玩家、弹幕
   遍历绘制（0x40e4d0 族）不在窗口内（stage1 环境无这些系统对象）；
   窗口覆盖 = ANM mgr 两池全部精灵（敌机/ECL 挂接精灵/ANM 装载预置精灵）。
3. **视/投矩阵**：设备创建时一份（0x45dc80），不随游戏 zoom 变化（真实游戏
   每帧 0x43b900 刷新——stage1 环境 gamestate 静止，视矩阵恒定等价）。
4. **render 或acle 与 stage1 事件流的差异**：ANM 指令族真实执行后 RNG 消耗
   与基线不同——渲染窗是独立基线（窗口帧数与事件流基线一致即可，事件流
   逐字节一致性属事件窗自己的验收）。

## 5. 自证（确定性）与实测基线

同参数连跑两次输出**逐字节一致**；每帧绘制数 > 0；窗口帧数与事件流基线一致。
`RESULT` 行是比对唯一裁决。

原版侧实测（容器内实际执行，2026-10 v1；**2026-10 对齐轮重导出**＝假设备
三缺陷修复后，见 §3 勘误）：

| 窗口 | 行数 | ops | quad | up | 零绘制帧 | 两次连跑 |
|---|---|---|---|---|---|---|
| 无输入 st01-draws.jsonl（v1） | 3600 | 301983 | 289456 | 12527 | 0 | cmp 逐字节一致 |
| 回放 st01-draws-replay.jsonl（v1） | 10663 | 774756 | 711279 | 63477 | 0 | cmp 逐字节一致 |
| 无输入 st01-draws.jsonl（重导出） | 3600 | 289456 | 289456 | 0 | 0 | cmp 逐字节一致 |
| 回放 st01-draws-replay.jsonl（重导出） | 11065 | 735235 | 735157 | 78 | 0 | cmp 逐字节一致 |

重导出后 up 归零/骤减的原因：极小环境下敌机 ANM 的出生位置来自随机/环境
变量（观察值 y≈-5745 恒在屏外），v1 基线里它们因记录宽高 NaN 使 0x467350
的视口裁剪比较恒假而**误画**；记录修好后被正确裁剪。敌机 up 的语义对齐
（位置种子链）与 stage1 事件窗 §4.4 的 RNG 消费差异同属后续工作面。

移植侧实测（容器内实际执行，2026-10；`tools/run-harness.mjs draws` /
`draws-replay`，WASI 无头链路——确定性最强：无浏览器/rAF/GL 时序）：

| 窗口 | 行数（帧号） | ops | quad | up | 零绘制帧 | 两次连跑 |
|---|---|---|---|---|---|---|
| 无输入 port-draws.jsonl | 3600（0..3599） | 293792 | 289456 | 4336 | 0 | cmp 逐字节一致 |
| 回放 port-draws-replay.jsonl | 11065（0..11064） | 750436 | 735098 | 15338 | 0 | cmp 逐字节一致 |

`draw_compare.py` 当前裁决（2026-10 对齐轮 3 后）：两窗 `first_divergence_
frame=61`，帧 61 的 op 数两侧已同为 9（8 神灵 quad + 1 敌机 up），分歧深
入敌机 up 的 verts 字段（`op[8].verts[0][0]: '149.5' != '139.5'`）。轮 3
的关键更正：**原版侧曾缺 0x43bbc0 战斗 gamestate 初始化**——视口块
gamestate+0xd8（UP 裁剪数据源）恒全零致全部 UP 精灵被裁（v1 修复后基线
的 ups 是误画、轮 2 基线的 ups=0 是误裁）；补驱动后基线 ups=139（无输入
窗）。敌机链已通：ECL 303/306 槽位 VM 创建（0x41fe80/case3）+ 每帧位置
跟随（0x41a8a0 把敌机 ECL 位置写槽 VM+0x574）+ UP 层口径（flush 时 watch
残值 56）。

神灵链已通：ECL `MapleEnemy` 的 `ins_308(1,91)`（0x41b260 case5 →
0x4243e0）→ 移植侧 `StageCommands` 307/308 真实现 + `StagePresentation`
排空创建（主池头入链），移植 quad 总数 289456 与原版**完全相等**；对齐轮
2 的关键修复：ANM/ECL 共享同一条 0x472c90 全局 RNG（状态 0x4dc31c 种子
0，装载期 2 调 = effect script23 模板 ins_120×2、每神灵 spawn 10 调）+
装载序 bullet→effect→stage + vec3 实参求值序 x→y→z + matrix_product
double 中间量 + 帧尾 SetTexture(0,0)（设备态清空、mgr+0xb78188 缓存不
失效——oracle f1-8 tex=None 的来源）。

剩余首分歧（帧 61 verts，下一轮工作面）：移植敌机运动模型首拍位移与
原版不符——GirlRedA01 生成 (-163.78,-16)，原版前两帧静止（+0x574 恒
-163.79），移植首拍已移动至 (-154.11,-16)（Δ≈9.67）。敌机运动学子系统
（ECL 404/405 速度/加速度解释、ins_23 等待期间的运动语义）此前仅有事件
操作码级验证、无位置级对拍——现在有了逐帧位置地面真值（槽 VM +0x574），
需按 0x418e00 族逐条对齐。次级：敌机 up 的 UV/顶点色链、ECL 313/316
（MBoss2 case7/10）未实现、回放窗自机弹 VM 的 RNG 消费（oracle 窗内无
自机弹系统）、stage 合并 bank 的文件 3+ 脚本号偏移（窗口内敌机均用文件
2，暂无影响）。

【事件窗回归注】replay 事件窗（vs stage1-oracle-replay.jsonl 48406 事件）
在 50433ebc（本轮改动之前）即已分歧于 frame 124（54816 事件；stash 本轮
改动后原样复现，非本轮引入）；stage1 无输入事件窗 10187 事件仍逐字节一致
（本轮实证）。

主要纹理（槽:entry:名）：8:2 `effect/eff_base.png`（神灵粒子）、
10:2 `enemy/enemy_aura.png`、10:1 `enemy/enemy2.png`、10:0 `enemy/enemy.png`
（杂兵族）、回放窗另见 11:0 `stgenm/stage01/enm1.png`、8:3 `effect/eff_aura.png`。
