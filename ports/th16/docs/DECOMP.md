# th16 反编译分区

导出：`node web/scripts/cpp/analyze.mjs` → `artifacts/th16/decomp/jp/`（`functions/<addr>.c/.asm`、`functions.json`（含调用图）、`imports.json`、`strings.json`、`blocks.json`）。Ghidra 12.1.3 headless，`ExportTh16.java` 后置脚本。

## 规模

| 分区 | 函数数 | 说明 |
| --- | --- | --- |
| 游戏代码 0x401000–0x46ffff | 1006（正文 431869 字节） | 全部可反编译 |
| CRT / 运行库 ≥ 0x470000 | 755 | 不还原，按语义替换（`operator new`、`memcpy`、`sprintf`、SEH） |
| thunk | 11 | — |

按 16 KiB 段的密度：0x40c000（67）、0x430000（69）、0x418000（64）、0x43c000（62）、0x46c000（58）为热区，对应 ECL 指令族、玩家/机体、标题菜单、ANM 管理器。

## 已还原锚点

| 原版地址 | 子系统 | C++ |
| --- | --- | --- |
| 0x402b70 / 0x402be0 | RNG 16/32 位（含临界区包装，标志 DAT_004c10b6） | `game/Rng.hpp` |
| 0x402220 | 资源加解密（奇偶位置分段，key+=step） | `game/ResourceCrypt.cpp` |
| 0x4572e0 / 0x4574b0 / 0x457120 | THA1 头部/目录/条目读取 | `game/Archive.cpp` |
| 0x457f00 | LZSS（13 位偏移 / 4 位长度 +3，字典 8192 起点 1） | `game/Lzss.cpp` |
| 0x46cdd0 / 0x46d0c0 / 0x46cf80 | ANM bank 装载 / entry 校验 / THTX 解码 | `game/AnmResource.cpp` |
| 0x46a3a0 / 0x46d1c0 / 0x46d3b0 / 0x46d8a0 | AnmManager 构造（0x1fff VM 池）/ 装载步（精灵表、模板预跑 `-1:` 块）/ 0x44 精灵记录 / uv 归一化 | `game/AnmManager.cpp AnmManager::load` |
| 0x45f980 | ANM VM 指令解释：低表 −1..209（0x462d8c/0x462cb8）、高表 301..613（0x462fa8/0x462e64），变量 0x45f610/0x45f2d0 | `game/AnmVm.cpp AnmInterpreter::run` |
| 0x4090f0 / 0x407a50 / 0x45f020 / 0x407b20 | VM 复位 / 尾部复位 / 模板初始化 / 模板拷贝 | `game/AnmManager.cpp` |
| 0x406190 / 0x406490 / 0x40d490 | 计时器推进（按 game_speed 累加）/ 设值 / 回退 | `game/AnmVm.cpp Timer` |
| 0x463b30 / 0x4033f0 / 0x4630f0 | 插值推进（模式 7 速度、0x11 加速度、8 Hermite、其余 31 种缓动）/ 缓动曲线 / 帧末速度与 uv 滚动 | `game/AnmVm.cpp` |
| 0x406380 / 0x46ed60 / 0x46eea0 | spawn / 子 VM（ins_500–506，列表位选择）/ 分离 VM（ins_507） | `game/AnmManager.cpp` |
| 0x46f600 / 0x46eab0 / 0x46e660 | 池 LIFO 分配 / 回收 / 递归标记删除 | `game/AnmManager.cpp` |
| 0x46e7d0 / 0x46e890 / 0x46e940 / 0x46ea00 / 0x46e340 / 0x46e490 / 0x46efa0 | 世界/UI 链表插入、逐帧 tick（layer 绘制链 +0x1c6fc30）、按 id 查找 | `game/AnmManager.cpp` |
| 0x407cb0 | AsciiManager：`ascii*.anm` 进槽 2；ins_300 负下标回退精灵 0x102 | `game/AnmManager.hpp kAsciiSlot` |
| 0x468490 / 0x46e750 | 按 VM / 按 layer 绘制；mode 0..3 为屏幕四边形 | `game/AnmRenderer.cpp` |
| 0x465c40 / 0x4660b0 / 0x465280 | 轴对齐角点 / 旋转角点 / 像素对齐·UV·颜色提交 | `game/AnmRenderer.cpp` |
| 0x406c40 / 0x406a70 | 世界位置（pos+offset+pos2）与窗口缩放 / root 链 | `game/AnmRenderer.cpp` |
| 0x43adc0 / 0x43b300 / 0x43b290 | 启动幕：装载 `sig.anm`；绘制链 0x44 帧计数；装载完成后请求 Title=4 | `game/GameSession.cpp` / `game/Supervisor.hpp` |
| 0x401730 / 0x401300 / 0x4013b0 / 0x401460 / 0x4015a0 / 0x4017a0 | Chain 条目 0x28 字节（prio/flags/cb/init/ctx）；更新链与绘制链按优先级插入、遍历返回码 | `game/Chain.cpp` |
| 0x45a8a0 | 帧计时：逻辑时间按 1/60 追到墙钟，再跑更新链、按跳帧计数跑绘制链 | `game/FrameClock.hpp` |
| 0x401d50 / 0x4018e0 / 0x405580 | 键盘位布局（Z=1 X=2 Shift=8 方向 0x10..0x80 Esc=0x100 Ctrl=0x200 Q=0x10000 S=0x20000 P=0x40000 Enter=0x80000）、手柄合并、边沿/连发 | `game/Input.hpp` |
| 0x44af80 / 0x44b5f0 / 0x44a700 / 0x44a680 / 0x44a790 / 0x44a650 / 0x44a560 / 0x44a5e0 | 标题更新：主菜单 case 0/1/2/4（装饰、Timer 0x78/0x82/0x13、光标、确定 0x80001 / 取消 0x102、画面表）；子项查找/中断/收起；画面与子状态切换 | `game/TitleScene.cpp` |
| 0x44fe20 / 0x4502c0 / 0x450af0 / 0x450ef0 | 难度 / 机体 / 子季节 / 练习关卡画面（VM 表 +0x2c0 按脚本号索引；面板 0x7e·0x7f / 0x96·0x98 / 0x97·0x99 / 0x71；季节 case 3 请求场景 7） | `game/TitleScene.cpp` |
| 0x449190 | 模式位（DAT_004a5bec bit4-5）与 Replay 槽复位 | `game/GameGlobals.hpp` |
| 0x402e70 / 0x418720 / 0x440c00 / 0x402de0 / 0x402e20 | MenuCursor：移动（wrap + 跳禁用）、设置、禁用、压栈、出栈 | `game/MenuCursor.hpp` |
| 0x46f340 / 0x46f3b0 | 清 flags_lo bit1（显示）并递归子树 | `game/TitleScene.cpp` |
| 0x46f510 / 0x46f0b0 / 0x46f130 | ANM 子树按脚本号查找；中断（含子 VM）；中断并立即 run | `game/AnmManager.cpp` |
| 0x467410 / 0x46b900 / 0x464dd0 / 0x464f10 / 0x45ba80 | ANM mode 8：世界矩阵 × 相机投影、±128 单位四边形锚点变体、父链旋转累加、混合/采样/寻址状态、设备缺省状态 | `game/AnmRenderer.cpp`、`game/AnmVm.cpp`、`sdl/Application.cpp` |
| 0x43cb10 / 0x43c780 / 0x43c710 | 相机组初始化（UI 相机 0x4c1560）、LookAtLH/PerspectiveFovLH 建矩阵、套用 view/proj/视口 | `game/AnmRenderer.hpp` AnmCamera |
| 0x44ad20 | 标题析构：摘两条链、卸载槽 0x10/0x11、落叶发射器（+0x73c）interrupt 1 | `game/GameSession.cpp` |
| 0x43ba40 / 0x43b3d0 / 0x43ce10 | Supervisor 注册 / 每帧泵 / 场景切换（+0x6f0/+0x6f4，DAT_004c17c4） | `game/Supervisor.hpp` |
| 0x401d50 / 0x405580 | 输入采样与边沿 | `game/Input.hpp` |
| 0x44aee0 / 0x44abc0 / 0x44ac70 | 标题创建；线程等启动幕 0xb4 帧（或 DAT_004c1800&0x180）后卸 sig、装 title*.anm、注册更新链 6 / 绘制链 0x45 | `game/GameSession.cpp` |
| 0x46f270 / 0x46d770 | 按槽杀 VM / 卸 bank | `game/AnmManager.cpp` |
| 0x42d700 / 0x42cb60 / 0x42d7b0 / 0x42dc50 / 0x42d200 | GameManager（0xb4，DAT_004a6dd4）：创建（flags|=4、更新链 0xf / 绘制链 2、装载线程）、装载（目前仅 StageManager + 背景 ANM）、每帧（timer 0 → 首帧 0x42dc50；DAT_004a579c/57a0++）、析构 | `game/GameManager.cpp`、`game/GameSession.cpp` |
| 0x4a22d0 | 关卡表 8 × 0xd4：std / ecl / bgm×2 / msg×4 / logo anm / 曲号 | `game/StageTable.hpp` |
| 0x40ac30 | STD：头 {i16 对象数, i16 面 VM 数, u32 实例偏移, u32 脚本偏移} + anm_name[0x80] + 对象偏移表；对象 {id, layer, flags, pos, size} + 四边形 {type, size, anm_script, vm_index(运行时), pos, w, h}；实例 {object, flags, pos}…0xffff；脚本 {time, op, size, args}；背景 ANM → 槽 3 + (stage & 1) | `game/Std.cpp`、`game/Stage.cpp` |
| 0x4097c0 / 0x409db0 / 0x40add0 / 0x409e50 / 0x40aed0 / 0x40b2f0 | StageManager（0x66f0，DAT_004a6d9c/a0）：相机默认 (0,0,-600)/(0,300,600)/(0,1,0)、更新链 0x11 / 绘制链 3、6；首帧面 VM init_from_template + run、vm_index 顺序分配；每帧 脚本 → 摇晃 → 面 VM → 层精灵 → 相机矩阵；面 VM 全停则清对象 bit0；interrupt（arg+7） | `game/Stage.cpp` |
| 0x40b3b0 | 关卡脚本（param_1 = this+0xc，跳转表 0x40bfbc 0..0x14，0xf/0x10 空）：0 stop、1 jmp、2 pos、3 posTime(t,mode,目标)、4 facing、5 facingTime、6 up、7 fov、8 fog、9 fogTime、10/11 bezier、12 摇晃模式（1,2,3,4,7,8,9：周期 0x200/0xc00/0x800/0xc00/0x800/0x400/0x200）、13 bgColor DAT_004c1b0c、14 层精灵、17 畸变、18 upTime、19 interrupt、20 半径² | `game/Stage.cpp` |
| 0x409f90 / 0x40af70 / 0x40c280 / 0x43c940 / 0x40d370 | 背景绘制：场地矩形清色、相机 0x4c168c（视口 (116,4,408,472)、LookAtLH(eye=pos+offset, at=eye+facing_norm) + PerspectiveFovLH(fov, w/h, 30, 8000)）、逐层 0..7：层精灵（无雾、Z 关）→ 实例 × 四边形（entity_offset = quad.pos + inst.pos，scale = wh / 精灵尺寸，雾 mode 8/0x18，ZENABLE=!(lo&0x2000)）；雾色 BGRA 浮点相加；顶点线性雾 f=(far−z_eye)/(far−near)，z_eye=1/rhw | `game/AnmRenderer.cpp` emit_stage、`game/Stage.cpp` draw_layer、`sdl/GraphicsDevice.cpp` screen_quad_fog |
| 0x441c60 / 0x440fb0 / 0x441740 / 0x441a50 | Player（0x2c828，DAT_004a6ef8）：创建（VM 0x4093f0、两实体 0x440ec0、memset）；初始化（pl0N.anm 槽 9 / pl0Nsub.anm 槽 0x1e、.sht ×2、更新链 0x16 / 绘制链 0x1d、脚本 0 → 自机 VM、0x440e40 置 (0,400)×128、速度 ×128、每级火力 100 → DAT_004a57e8/ec、季节阶梯 0x43deb0 {0,100,0x82,0xa0,200,0xfa,300,0}、半径 DAT_00492c98/88/68 ×0.5、三组判定矩形、滞后 0x1e）；首帧复位（状态 1、计时器、flags&~9、kill 特效 VM、Gui 残机、子机 interrupt 2、0x4440e0） | `game/Player.cpp` init / reset、`game/GameSession.cpp` create_player |
| 0x443790 | .sht：+2 组数、+4/8/c 半径（被 exe 表覆盖）、+0x10..0x1c 四档速度、+0x20 最大等级、+0x24 每级火力、+0x40/+0xe8 子机位置 ×21、+0x190 组偏移（基 +0x1b8）、0x58 字节条目（首字节 <0 结束，+0x28..+0x34 回调表 DAT_004919c0/19a0/4a6f04/1980） | `game/ShotTable.cpp` |
| 0x443720 → 0x442560 | 自机每帧：状态机 0 入场（0xf000 − t×0x2800/0x3c）/ 1 正常（炸弹 0x40dda0 + 0x441cf0）/ 2 复活等待 / 3 / 4 被弹；伤害源 ×257；无敌计时倒数（0x16638，每 3 帧 color2 0xff0000ff + flags_lo 0x20000）；speed_mult 复位、推力 DAT_004d9dc4；自机 VM run；flags 0x10 缩放模式（0x4171c0）否则 scale 1 + 判定矩形（收点框 ×0.5）；三计时器 tick；射击门限（Gui+0x1c8==0 && 敌人数≠0 && !过关 && Timer3>0x13 && !(flags&0x14)）→ 0x4455d0，否则射击计时复位；0x4456d0 自机弹 | `game/Player.cpp` update |
| 0x441cf0 | 移动：方向表 DAT_00492c20（0x50→5 UL、0x60→7 DL、0x90→6 UR、0xa0→8 DR、单向 1..4）；低速 = 敌人数≠0 && Timer2≥4 ? bit3 : 0（否则滞后=0x1e）；低速判定点 effect.anm 0x1a layer 0xe（scale2 = 1 或 2(scale−1)+1，flags_lo|8）；v = (速度×dir − (int)(推力×−128)) × 倍率；精灵 1/3 开始左/右、2/4 回正；pos += v×游戏速度；钳制 ±0x5c00 / 0x1000..0xd800；子机 0x442380 ×4 + ×8；收拢 >0x1d 帧清子机数；第二特效 0x1b 计时 | `game/Player.cpp` move |
| 0x442380 / 0x4440e0 / 0x426160 / 0x46f1c0 / 0x46f220 | 子机：目标 = 定点 pos + (低速?焦点偏移:偏移)，收拢时目标 = pos 并 >0x1d 帧后 interrupt 1；滞后 >0x1d 时 Δ = (目标−当前)×滞后/100，Δ=0 或 snap 时吸附；VM entity_offset = 当前/128。重建：等级 = 火力/每级；火力 < 上限 → 前插 VM interrupt 1，否则 kill 并 0x426160 前插（DAT_00492c10[机体]，layer −1，creation_flags 2）；主子机 DAT_00492c00[机体] 脚本、位置表 DAT_004a5e4c[机体][等级]+i × .sht ×128，内联生成 = 0x406380 路径，entity_offset.y = −32；子季节子机 DAT_00492be0[季节] / DAT_004a5dac，数量 = 量表 ≥ 阈值 DAT_004a583c..50 个数。kill：flags_hi=(hi&~0x40)\|0x20 递归子树（0x4000000 保护） | `game/Player.cpp` update_options / setup_options、`game/AnmManager.cpp` spawn_front / kill / kill_tree |
| 0x4455d0 / 0x440d50 / 0x43ac80 | 射击计时：Timer<0 且按 Z → Timer2 set 0、Timer set 0；Timer<0xe tick，否则松键 set −1 / 按住回退 0xe；Timer2 >0x76 → 松键 −1 / 回退 0x77；发弹 0x445470 | `game/Player.cpp` shoot / start_shot_timer、`game/AnmVm.cpp` Timer::rewind |
| 0x445470 / 0x445360 | 发弹：组 = 火力/每级火力（低速 +1+最大等级），组内项在 `timer % period == phase`（或 period2 对 Timer2）时出弹；子机组 = 季节子机数（season_option_count），每个子机出一发（shot_id |= 0x10000）；类型 2 项受冷却表 +0x165f8[k + sub*8] 限制；弹位 = 池内首个 flags==0 | `game/Player.cpp` fire_sets、`game/ShotTable.hpp` ShotEntry 访问器 |
| 0x444e10 / 0x444b20 / 0x445ed0 / 0x447450 | 自机弹创建：计时 (0, 0, −1)、伤害 i16、判定宽高、flags bit1 = 低速；位置 = 自机 pos 或子机定点/128；角度 <995 环绕、≥995 用子机角、≥1000 随机 ±π/12（视觉 RNG）且速度 +2·rand；update_velocity 后 pos += offset − velocity；VM = 脚本 (+5 子机)，VM flags_hi&0x80 → flags_lo|=4，rotation.z = 原始角；伤害源 0x444b20（从上次下标后首空位，1 起编号；类型 1、f7c=10000000、计时 9999999、owner=弹下标）；生成回调 1 target=0 / 4 伤害源类型 2；sfx 0x45e1f0（组内首项 0，其余 −1 不入队） | `game/Player.cpp` spawn_bullet / alloc_damage_source |
| 0x4456d0 / 0x445ee0 / 0x447480 / 0x468c00 | 自机弹每帧：推进回调（1 追踪：无敌时 speed=min(speed+0.1,16)；5：active==1 时 speed+=0.5）→ update_velocity / integrate → VM 查找 → 在界 = 类型 2 ‖ 计时<0xf ‖ 四角（0x468c00：模式 0/2/3 轴对齐、1 旋转 + world_position）任一严格在 (0,0)-(384,448) 内（DAT_004d9d38/3c 恒 0）→ 同步伤害源 / VM 位置 / 旋转、计时 tick；否则 kill VM、回收弹与伤害源 | `game/Player.cpp` tick_bullets / release_bullet、`game/AnmRenderer.cpp` vm_corners |
| 0x402ff0 / 0x403110 | 运动体 0x44 字节：模式 0 极坐标速度→velocity、pos += velocity；模式 2 半径/角度；坐标 floor 到 1/100（`floor(x*100)/100`，0x488a00 SSE floor）；极坐标以 double 乘后单次舍入 | `game/Motion.cpp` MotionState |
| 0x442560 伤害源段 | 伤害源 ×256（0xd114，0x94 字节）每帧（无敌处理之前）：运动 update、f04 += f08、angle = wrap(angle + angular_speed)、f84 = 0、计时回退，<1 → 失活 | `game/Player.cpp` update |
| 0x447fd0 / 0x448130 / 0x418650 | Replay 记录（更新链 0x10）：DAT_004a52c8 = DAT_004a50b0 低 16 位（自动低速选项 DAT_004c12cc&0x200：按 Z 且火力≥10 时加 bit3），0x418650 与 0x405580 同口径边沿/连发，每 30 帧记一次帧率字节；回放 0x448130 逐帧取 6 字节 | `game/GameSession.cpp` create_player 记录链、`game/Input.hpp` |
| 0x474530 / 0x474740 / 0x472030 / 0x471db0 / 0x474430 / 0x473e40 / 0x473ef0 | ECL 资源与通用解释器：SCPT 头 + ANIM（装入槽 10+i）/ ECLI 递归包含、合并子程序表二分查找、16 字节指令头、操作码 0..0x5d（栈帧 40/41、表达式栈 42..45、算术/比较/逻辑 50..77、PostDecrement 78、三角/极坐标/向量旋转 79..90、插值槽 91/92、随机极坐标 93）、同步调用 11 与返回 10、异步线程 15/16、引用掩码与栈偏移 -1..-100 / 全局变量 `<= -101` | `game/Ecl.hpp`、`game/Ecl.cpp` |
| 0x41b340 / 0x41ae70 / 0x41b3d0 / 0x41aa70 / 0x41d2e0 / 0x41bb50 / 0x41c1f0 / 0x41dcb0 / 0x423050 / 0x423810 / 0x423f80 / 0x424c10 | EnemyManager（0x190，DAT_004a6dc0）与 Enemy（0x574c）：装关卡 ECL、登记更新链 0x1a、首帧 0x42dc50 生成 `"main"`；生成 0x41aa70 立即执行首帧 0x41d1e0 并保留 `0x40000` 使首个管理器回合只清位（隔帧再积分）；每帧 0x41bb50 先推进角度/速度/位置插值，再更新 `motion1/motion2`、0x41c1f0 合成 `pos += (m1+m2-pos)` 后 `floor(100)/100` 与 `0x20000` 边界框钳制、按主 VM 精灵尺寸（op 306 slot=0 时置 32×32 与 `0x100000`）判定进出场地；操作码 300..305 生成、400/401/404..407/412 运动、500..505/511..515/520/522/535..539/548/568 属性、1000 掉落；`enemy_count` 自动同步 `PlayerWorld.enemy_count` | `game/Enemy.hpp`、`game/Enemy.cpp`、`game/GameSession.cpp`、`game/GameManager.cpp` |
| 0x41c330 / 0x445a30 / 0x403d30 / 0x445d40 / 0x445e20 / 0x4474a0 / 0x447270 / 0x445ee0 / 0x425240 / 0x41a6d0 / 0x41d520 / 0x41d700 | 敌人受击与自机弹命中：0x445a30 遍历活动伤害源，0x403d30 旋转矩形与圆相交，f84 防同帧重入，kind 1 调 0x445d40（默认 0x445e20 置 z=0.1、VM interrupt 1、active=2、speed×0.125、清伤害源；文 hit_callback 5 调 0x447270 生成 effect.anm 0x98），kind 2 调 0x4474a0（文低速充能弹返回原始伤害并将弹伤害置 1 持续穿透），总伤害按 `sht.damage_cap` 封顶；0x41a6d0 扣血（符卡 `life_flags&1` 下 `life_budget -= dmg; life = (life_budget - phase_life*7)/7 + phase_life`）；`life < 1` 时 0x41d520 播死亡音效 `(id&1)+3`、按 `season_drop_timer` 算季节道具数并经 0x41d700 消耗 `1 + 2*count` 次 `visual_rng`；0x445ee0 + 0x425240 灵梦追踪弹索敌（256 范围内最近敌机）与角差加减速转向 | `game/Player.cpp`、`game/Enemy.cpp`、`game/AnmManager.cpp` |
| 0x411dd0 / 0x411a30 / 0x411b70 / 0x412860 / 0x411e70 / 0x412670 / 0x414da0 / 0x412cb0 / 0x412a60 / 0x443840 / 0x417140 / 0x413860 / 0x414ec0..0x4162d0 / 0x416840 / 0x416a00 / 0x416f40 / 0x49f2e0 / 0x490ec0 | BulletManager（0xa0f2e0，DAT_004a6da0）与 Bullet（0x1478 × 2000）：创建（更新链 0x1c / 绘制链 0x25、sprite_mapper[1]）、复位（空闲链按槽号头插，1999 在链头）、每帧遍历活动链（标志 bit0/bit2 暂停门控、0x400 冻结只同步 VM）；单弹 0x411e70：状态 1 常规 / 2 雾化生成（计时 ≥ 0x10 转 1）/ 3 被弹溶解 / 4 消弹 / 5 立即溶解，Ex 步骤轮询（0x413860 解释 0x0..0x1f：初速 / 加速 / 缓动 / 角加速 / 停转 / 反弹 / 穿墙 / 等待 / 子弹 / 换弹种 0x200 / 移动 / 附加速度 / 判定 / 缩放 …）、出界回收（精灵半宽高 + 0x20 边距）、0x4124b0 自机判定（矩形 0x4438c0 / 圆 0x4439e0：0 无事 / 1 被弹 → 0x443f10 + 中断 1 + 消弹特效飞散 / 2 擦弹 → 0x444cf0 + 得点道具 0x10）；发射 0x414da0：EtShooter way×stack 展开、瞄准模式 0..7（6/7 消耗 DAT_004a6d88）、角速度、主 VM / 附加 VM（0x45f160 内嵌 VM 绑脚本）、消弹脚本按方式 0..10 选择（方式 1 查 0x490ec0 色表）、判定桶 0..5；消弹 0x416840/0x416a00/0x416f40（屏内 ±32 转道具，模式 2/4/5 消耗 DAT_004a6d88）；ECL 600..641（0x42085b..0x4226d0）发射器属性 / Ex 步骤写入（609..612，步骤长度表）/ 按秩三档·五档·线性选参 / 复制 / 消弹 + 道具吸引 | `game/Bullet.cpp`、`game/Bullet.hpp`、`game/Enemy.cpp` case 600..641、`game/Player.cpp` hit / graze |
| 0x4314a0 / 0x431330 / 0x42cb00 / 0x4316b0 / 0x431510 / 0x431720 / 0x431760 / 0x41aa40 / 0x413860 / 0x431b30 / 0x432f40 / 0x431fe0 / 0x432dc0 / 0x432c20 / 0x432620 / 0x433510 / 0x443af0 / 0x404220 / 0x403ec0 / 0x434cd0 / 0x434730 / 0x433860 / 0x435050 / 0x436fd0 / 0x4352f0 / 0x435610 / 0x436c70 / 0x436670 / 0x435880 / 0x4370a0 / 0x431190 / 0x4377d0 / 0x438cb0 / 0x437ee0 / 0x438370 / 0x439730 / 0x437cf0 / 0x43a620 / 0x43a2f0 / 0x4397d0 / 0x43a860 / 0x431af0 / 0x4319e0 / 0x431a70 / 0x431950 / 0x431fa0 / 0x43a840 / 0x4252d0 / 0x4253b0 / 0x46f300 | LaserManager（0x5ec，DAT_004a6ee0；更新链 0x1b / 绘制链 0x23）与四类激光（直线 LaserData 0x5d4 / 照射 LaserInf / 曲线 LaserCurveInf 0x1568 / 占位 LaserBeam）：池上限 0x200、句柄自 0x10000 递增；0x431510 逐条推进时先取 next（尾条同帧派生的子激光下一帧才跑），延迟销毁为 +0xc 两位计数（标记次帧即移出）；直线：伸长 → 行进 → 出界回收，Ex 解释 0x431fe0（加速 / 折返 / 墙面镜像派生 / 重射 / 免疫 / 换弹种），判定跳过前 1/10 取 4/5 长，消弹每 16 像素采样命中表后切段派生；照射：预告 3 → 展开 4 → 维持 2 → 收束 5，角速度旋转、跟随 Boss 0，只认免疫 / 终止 / 混合 Ex，消弹按命中段派生直线激光；曲线：节点数 = op 701 telegraph，按采样时刻 frame_timer − i 由运动律段链（匀速 / 匀加速 / 角速度）重算，头节点 0x437ee0、后续节点由前一节点倒推 0x438370，Ex 4 / 8 追加段而非改速度，矩形消弹前导命中回退采样时刻、≥ 4 连续未命中派生继承段链的子曲线；擦弹点 z 在三处判定里都是未初始化栈值（0x43ad00 极坐标只写 x/y），移植取 0、oracle 在 0x444cf0 入口同口径归零；ECL 700..714（case 0x99..0xa7）复用发射器槽并把长度 / 时序放在 +0x340 参数区，545 = 0x4319e0 全消无视免疫；复活入场 0x442560 以死亡坐标 pos_copy 为心、半径 (t×512)/30+64（单精度）切断激光，再按 1/4 半径无视免疫；敌机每帧钩子表 DAT_004917b8（op 632 / 637 / 639）：1 = 0x4252d0 F0 半径内 Ex 0x8000 参数 1 的弹跳到 Ex 第 8 步、2 = 0x4253b0 火力与季节量表归零并 0x4440e0 重建子机；ECL 502 / 503 在隐藏位 0x20 变化时对全部 VM 槽 0x46f340 / 0x46f300 整棵树隐藏 / 显示 | `game/Laser.cpp`、`game/Laser.hpp`、`game/Enemy.cpp`、`game/Bullet.cpp`、`game/Player.cpp`、`game/Gui.cpp`、`game/GameSession.cpp`、`game/AnmManager.cpp` |
| 0x42f440 / 0x42f0b0 / 0x42f260 / 0x4184a0 / 0x4308f0 / 0x42f4e0 / 0x430960 / 0x430c90 / 0x430d10 / 0x430d90 / 0x4307a0 / 0x430100 / 0x430620 / 0x4304a0 / 0x4303a0 / 0x43df70 / 0x43dff0 / 0x43dfb0 / 0x430dc0 / 0x43de50 / 0x443840 | ItemManager（0x1c972ec，DAT_004a6ddc）与 Item（0xc78 × 0x1258）：创建（更新链 0x1d / 绘制链 0x21 非季节 + 0x13 季节）、复位（普通池 600 / 得点池 0x1000 各自按槽号头插空闲链 → 最高槽先出）、每帧遍历 0x42f4e0：态 1 下落（重力 0.03、终速 2、y>472 回收）/ 2 上抛（脚本表 DAT_004917f8）/ 3 季节减速（op 1001 / 击杀掉落 `unit()*1.9f+0.2f` x87）/ 4 吸引（基准 sht+0x08=5、每帧 +0.2 至 12；收点线 y<128、魔理沙 3 号 148）/ 5 收点圈（低速时擦弹框，速度 = 基准/3）/ 6 得点池延迟出现（spawn_count % 4/8/16/32）；拾取框 → 结算：火力 0x430100（满火力折得点 × 高度比例）、得点 0x430620（[line, line+450] 线性）、大火力 0x4304a0（满火力 2 万双计）、满火力 0x4303a0、残机 0x43df70 / Bomb 碎片 0x43dff0 / Bomb 0x43dfb0、季节 0x43de50（量表 +1、跨阈值重建季节子机 0x4440e0）；0x4440e0 季节子机循环沿用主子机游标（激活当帧 x/y 取第 i 个子机槽偏移、target 写回该槽）；ECL -9989 / -9956 / -9955 = 0x443840 自机方向角、-9971..-9966 = 运动 1/2 角度·速度·半径、-9963/-9962 Boss 坐标、-9958 位移方向、-9931 最近生成 id；op 507 = 掉落表登记（0 单掉 / 1..15 数量 / 16 季节数） | `game/Item.cpp`、`game/Item.hpp`、`game/Enemy.cpp`、`game/Bullet.cpp`、`game/Player.cpp`、`game/GameSession.cpp` |

Enemy + ECL 与受击战斗的还原证据：`tests/enemy.test.mjs` 以 `tools/oracle/game-oracle.py --enemies` 为 oracle，对 8 组场景（st1 Normal 前段 400 帧、Lunatic 前段 240 帧、BossSpell 200 帧、无桩低速移动 250 帧、琪露诺移动射击击杀妖精 140 帧、灵梦春追踪弹索敌转向击杀妖精 140 帧、琪露诺射击打 BossCard1 150 帧、文低速充能穿透弹 + 主弹打 BossCard1 150 帧）的 `enemy_digest` 与 `player_digest` 逐帧 FNV-1a 64 完全一致（`tests/golden/enemy.json`）；wasm32 同摘要，3600 帧 0.21s。

BulletManager 的还原证据：`tests/bullet.test.mjs` 以 `tools/oracle/game-oracle.py --enemies --bullets` 为 oracle（oracle 另记录 0x45e1f0 / 0x45e150 两种 sfx 入口，快照只保留 Player 代码段 0x440000..0x449000 的调用者；DAT_004a6d88 种子与 DAT_004a6d8c 调用计数随帧输出），对 3 组场景（st1 Lunatic 前段 211 帧含自机 / 敌人 / 敌弹三摘要、同场景 240 帧敌人 + 敌弹、st1 Normal BossSpell 回放槽 0 246 帧三摘要）每帧活动敌弹链表 22 字 × N（槽号、flags、状态、弹种、颜色、坐标、速度向量、速率、角度、判定、缩放、Ex 下标 / 标志、计时器、主 VM 精灵号）FNV-1a 64 逐帧一致；wasm32 同摘要。Lunatic 截止在首次被弹后决死窗口结束（第 212 帧进入状态 2，0x443cd0 待接）之前；BossSpell 截止在 Boss 入场充能特效（effect.anm 155+ → ins_508 → EffectManager 种类 1 消耗回放 RNG）影响 op 412 之前。ECL 异步线程（0x474430 / 0x473bc0）修正为：生成时以 0x471db0 建调用帧传参（op 16 跳过线程号），链表遍历先取 next，再执行，本帧新生线程下帧起跑。

Player 的还原证据：`tests/player.test.mjs` 以 `tools/oracle/game-oracle.py`（Unicorn 启动整局原版：链管理器、全部管理器与 ECL，IAT 全桩；不跑绘制链）为 oracle，对 5 组输入脚本（灵梦春 / 琪露诺夏 / 魔理沙冬 第 1 关、文秋 第 2 关满火力）230..250 帧的 Player 规范字段（状态、定点/浮点坐标、三计时器、方向、上次移动、低速与判定点 VM 有无、速度、滞后、火力等级、标志、无敌与射击计时、三组矩形与半径、缩放、12 子机）与自机 VM 0x568 字节逐帧一致（golden `tests/golden/player.json`，harness `game-run` / `game-digest`；VM id 只比有无）；wasm32 同摘要。截止帧取在首次被弹 / 首次拾取道具之前（Bullet / Item 未接入）；敌人数与对话态以 oracle 快照喂给 `PlayerWorld` 桩。

StageManager 的还原证据：`tests/stage.test.mjs` 以 `tools/oracle/stage-oracle.py`（Unicorn 直跑 0x409db0 / 0x40add0 / 0x409e50，text.anm 在槽 0、背景 ANM 在槽 3+(stage&1)，桩 0x46d020 与 d3dx9 的 D3DXVec3Normalize）为 oracle，对 7 关各 6000 帧的相机（pos/facing/up/facing_norm/offset/fov）、脚本计时与偏移、摇晃、雾、层精灵号、半径²、全部面 VM 与 8 个层精灵 VM 的 0x568 字节逐帧一致（golden 摘要 `tests/golden/stage.json`，harness `stage-digest`）；wasm32 构建摘要相同。

LaserManager 的还原证据：`tests/laser.test.mjs` 以 `tools/oracle/game-oracle.py --enemies --bullets --items --lasers`（`--lives 8 --refill-lives`）为 oracle，对 6 组 Boss 符卡练习场景（st3 Card2 曲线 900 / 1800 帧、st5 Card1 直线 1800 帧、st6 Card4 钩子 1 / Card9 照射 + 钩子 2 各 1800 帧、st7 Card7 照射 1800 帧；带每 150 帧换向的移动输入，覆盖擦弹 / 被弹 / 复活扩圆消弹 / 矩形消弹切段派生）逐帧激光池（种类、生命周期位、状态、句柄、位置 / 速度 / 朝向 / 长度 / 宽度、Ex 游标与标志、三组计时器、曲线全部节点）与同场景 player / enemy / bullet / item / spell digest 全部一致。

ANM VM 的还原证据：`tests/anm-vm.test.mjs` 以 `tools/oracle/anm-oracle.py`（Unicorn 直跑上述原版函数）为 oracle，对 54 个 ANM 文件 1887 个脚本各 60 帧的 VM 结构体（0x568 字节纯数据区）与 36 条 layer 绘制链逐字节一致；wasm32 构建与本机构建输出逐字节相同。绘制几何：`tests/anm-draw.test.mjs` 锁定 `sig.anm` 稳态四边形（512×480 + 128×480 @ layer 26，UV v=0.9375，淡入至白）。场景时序：`tests/supervisor.test.mjs`。

## 已定位、待还原

| 原版地址 | 含义 |
| --- | --- |
| 0x43adc0（其余步骤） | 启动幕后续：`text.anm` → `../../bgm/thbgm.fmt` → `thbgm.dat` → score 对象（0x4497e0）；目前只完成 sig 上屏与向 Title 切换 |
| 0x43d140 / 0x43d2f0 / 0x43d820 / 0x43d6f0 | 渲染目标合成链（绘制链 1 / 0xf / 0x38 / 0x2b）：场地先画进 RT DAT_004c127c（中心 (320,240)），再由 front.anm RT 精灵 VM 0x4c128c/0x4c1298 贴回后台缓冲（场地中心 (224,240)）；移植版暂以 GL 视口裁剪到 (32,16,384,448) + x−96 等效，后端尚无 RT |
| 0x40a410 / 0x40a7d0 / 0x40c4a0 | 绘制链 6（ANM 层 0x20/0x21 用 3D 相机）、对象包围盒视锥剔除、畸变效果（ins 17） |
| 0x44c570 / 0x451750 / 0x452330 / 0x4546f0 / 0x4545a0 / 0x4553d0 | 标题余项（Option / Replay / Player Data / Music Room / Manual / Spell Practice）；0x44af80 开头的 demo 录像触发；0x44a850 / 0x44a8e0 / 0x44a800 存档判定；0x41a390 + 0x418ba0 过场；0x43c470 BGM 淡出 |
| 0x46d020 | AnmManager::load_file(slot, name)（同步等待装载线程） |
| 0x418af0 / 0x405700 | EffectManager（DAT_004a6db8）：ins_508 按效果种类表 DAT_004a2250 挂 update/draw 回调；种类 1 的 update 回调维护环形顶点缓冲并生成 `effect.anm` 0x99。`effect.anm` 155–196 依赖它，暂不在 ANM golden 内 |
| 0x491b1c 表 | ins_300/301 的 sprite_mapper 回调（VM +0x5dc 非零时由玩家/敌机子系统重映射精灵号） |
| 0x41dcb0 | ECL 指令分发 |
| 0x44ac70 | 标题场景初始化 |
| 0x4497e0 / 0x4403c5 / 0x43b480 / 0x43aeae | score 文件 / replay 文件 / ver 文件 / thbgm.fmt |

未定位：Bomb / 季节解放（Release）、Gui HUD、Spellcard 余项、敌机光环特效 0x418c70（ECL 629）；敌机、子弹、激光、道具、玩家与机体已还原。Supervisor、Chain 表、帧计时、输入、标题主菜单与难度/机体/季节/关卡画面已还原；Game 场景（7）与标题余项待接。全表见 `web/cpp/rebuild/AddressMap.hpp`。

## 与 th15 的关系

th16 是 th15 引擎的直接后继：THA1 密钥表、RNG 算法、ANM 版本 8 一致；差异是 RNG 外层临界区、SSE2 标量单精度编译、季节系统与 4 机体 × 4 子季节。th15 还原件可作为阅读参照，但每个函数仍以 th16 的 `.c/.asm` 为准。
