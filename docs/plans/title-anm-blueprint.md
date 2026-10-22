# th13 主菜单 ANM 蓝图（title.anm / title_v.anm 脚本号总表与重锚方案）

> 目的：把主菜单重做所需的全部 ANM 锚定固化为可执行文档。一切数值出自
> `artifacts/reference/dumps/anm-list.txt`（下称 **dump**，行号直接引用）与移植侧源码
> （`web/cpp/` 相对 `eagler-touhou/ports/th13/`，行号引用）。字条/图标为烘焙位图，
> 内容语义按序号引用；无法从数据解出的一律标「待锚定/待人类辨认」。
> 事实源排序见 §六：unicorn 标题窗 oracle 实测 > 本蓝图。

## 〇、机制前提（从移植侧源码实证，蓝图全部序列依赖这两条）

1. **中断传播**：`AnmRegistry::interrupt`（AnmRegistry.cpp:95，注释锚 0x46fca0）把
   pending 标签同时下发给**被中断 VM 的全部直接子 VM**。因此 `interrupt(root, L)`
   的语义 = 根自身 + 每个子脚本各自扫描自己的 `ins_5(L)`：有该标签者响应、无者继续。
   th15 的 `interrupt(0, 3)` / `interrupt(0, cursor+7)` 调用形状在 th13 **原样成立**
   （标签号不同）。`ins_5(id)` 即中断标签定义；缺失且无 `-1` 回落标签时该 VM 无动作
   （AnmInterpreter.cpp:57 扫描逻辑）。
2. **暂停不冻结插值**：VM 停在 `ins_3()`（待中断，AnmInterpreter.cpp:109）期间
   `advance()` 仍逐帧推进插值器（AnmUpdate.cpp:8，由 tick 的 idle 路径调用）。
   因此白态字条 spawn 时的 `ins_409(15,0,255)` 会**自动完成 15 帧淡入**，无需任何中断。

指令速查（AnmInterpreter.cpp 行号）：`ins_300` 选精灵:117 / `ins_304` 层:121 /
`ins_400` 位置:135 / `ins_403` alpha:138 / `ins_404` 颜色:139 / `ins_407/408/409/412`
位置/颜色/alpha/缩放插值:142-147 / `ins_431` 时率锁定:164 / `ins_500` 建子 VM:172 /
`ins_5` 标签+`ins_7` 返回 / `ins_200` 计数回跳:115 / `ins_6(N)` 计时快进。

bank 表（Application.cpp:16-18，常驻）：ascii=**2**、effect=8、title=**16**、
title_v=**17**、help=**19**。title.anm 共 23 entry / 242 脚本（script0-241），
精灵号全文件连续（每精灵自带纹理归属，AnmInterpreter.cpp:37）。

## 一、脚本号总表

### 1.1 两类字条形态与公共 label 语义（主/难度/角色菜单同构）

| 形态 | 初始行为 | label 语义 |
|---|---|---|
| 灰态（层 26，色 192,192,192） | spawn 即 `ins_3()` 待机，alpha=0 **隐藏** | `7+k`/`17+k` 入场（5f 淡入×2→±1px 抖动 10f→192↔255 脉动 20f×10 循环，dump L17342-17453）；`2` 确认：64↔255 闪 20f→缩 0.5 飞至 (256,332) 停（L17421-17423）；`3` 淡出 5f；`6` 闪 20f 后淡出销毁（L17447-17453）；`27` 变黑 4f；`28` 恢复 192 4f；`29` 隐藏 1f；`30/31` **空操作**（`ins_7` 立即返回，L17393-17394） |
| 白态（层 25，色 192,192,192） | spawn 自动 15f 淡入（前提 2） | `3` 淡入显示 5f；`7`/`17` 淡出隐藏 5f（L18509-18514）；`27/28/29` 同灰态但 29=黑+alpha160；`6` 待 10f 销毁；`1` 淡出 20f 销毁；`30/31` 空操作 |

层 26 绘于层 25 之上（灰态盖白态）。选中机制（数据推定，oracle 锁定后回写）：
整体 `interrupt(root,3)` = 全部白态淡入+灰态淡出（未选中外观）；随后
`interrupt(root,7+k)` = 第 k 项灰态入场脉动+白态 k 淡出（选中外观）。

### 1.2 主菜单：根 script0（dump L17223）

- 锚点 **(70,280)**（L17227），仅 label 1（L17249-17252）；spawn 子脚本 3-22（L17228-17247）。
- **10 个菜单项**（每项灰白两条，内容为 title01.png 128×16 位图字条，左列/右列）：

| 项 k | 灰态(层26) | dump 行 | 白态(层25) | dump 行 | sprite 灰/白 | y |
|---|---|---|---|---|---|---|
| 0 | script3 | L17342 | script13 | L18482 | sprite0/sprite1 | 0 |
| 1 | script4 | L17456 | script14 | L18525 | sprite2/sprite3 | 18 |
| 2 | script5 | L17570 | script15 | L18568 | sprite4/5 | 36 |
| 3 | script6 | L17684 | script16 | L18611 | sprite6/7 | 54 |
| 4 | script7 | L17798 | script17 | L18654 | sprite8/9 | 72 |
| 5 | script8 | L17912 | script18 | L18697 | sprite10/11 | 90 |
| 6 | script9 | L18026 | script19 | L18740 | sprite12/13 | 108 |
| 7 | script10 | L18140 | script20 | L18783 | sprite14/15 | 126 |
| 8 | script11 | L18254 | script21 | L18826 | sprite16/17 | 144 |
| 9 | script12 | L18368 | script22 | L18869 | sprite18/19 | 162 |

灰态入场 label 为 `7+k`/`17+k`（逐项偏移，k=0→7/17 … k=9→16/26，见各脚本）。
**项数证据**：根 spawn 20 条=10 行×2 态；字条纹理共 50 条 128×16（dump L17148-17197）。
逐项文字内容为烘焙位图 → **待人类辨认**（可用 `artifacts/reference/golden/` 标题帧比对）。

### 1.3 难度菜单：根 script1（L17256）

- 锚点 **(70,312)**（L17260），仅 label 1；spawn 顺序
  23,24,25,26,27,33,34,35,36,41,42,43,44,28,29,30,31,32,45,46,47,48,37,38,39,40（L17261-17286）。
- **5 个难度名项**（灰 L18912/19026/19140/19254/19368，白 L19482/19525/19568/19611/19654；
  sprite 26/28/30/32/20 与 27/29/31/33/21，y=0/18/36/54/72；入场 label `7+d`/`17+d`，d=0..4）。
  内容位图（Extra 疑为第 5 行 sprite20/21，第 10 行条）→ 待人类辨认。
- **2 行 4 位数字组**（16×16 图标精灵 sprite50-71，两行各 11 枚，L17198-17219）：
  行 0（y=0，x=70/82/94/106）：灰 script33-36（L19697/19811/19925/20039，sprite50×3+sprite60）
  / 白 script37-40（L20153/20196/20239/20282，sprite61×3+sprite71）；
  行 1（y=18）：灰 script41-44（L20325/20439/20553/20667）/ 白 script45-48（L20781/20824/20867/20910）。
  入场 label `7+r`/`17+r`（r=行号 0/1，与难度名行同号联动）。
  内容为位图图标/数字（sprite50-60=灰行、61-71=白行），**数值语义待锚定**
  （静态 sprite 选择，动态换字机制未见——推测由 supervisor 换 sprite 或仅静态装饰）。
- 现状代码沿用 th15 的「Easy..Lunatic 4 项+Extra 禁用」（TitleSelectionMenu.cpp:39）
  与 dump 的 5 行结构不符；Extra 解锁位**待锚定**（建议先按 5 项全开对 oracle）。

### 1.4 角色菜单：根 script2（L17295）

- 锚点 **(70,312)**（L17299），仅 label 1；spawn 49-82 顺序（L17300-17333）。
- **7 个角色名项**：灰 script49-55（L20953/21067/21181/21295/21409/21523/21637，
  sprite 34/36/38/40/42/32/18）、白 script56-62（L21751/21794/21837/21880/21923/21966/22009，
  sprite 35/37/39/41/43/33/19），y=18k，入场 label `7+k`/`17+k`（k=0..6）。
  7 项=4 角色+3 项其他（位图内容**待人类辨认**；不可凭记忆断言）。
- **5 行 2 位数字组**（x=124/136，y=0..72）：灰 script63-72（L22052-22964，sprite50）/
  白 script73-82（L23192-23579，sprite61），入场 label `7+r`/`17+r`（r=0..4，
  仅行 0-4 有数字，行 5-6 无）→ 数值语义**待锚定**。

### 1.5 背景：根 script89 与两层拼贴（L23903-24056）

- 根 script89（L23922）：alpha=0，spawn 顺序 **92,93,91,90**（L23924-23927），仅 label 1。
- 底图森林（title00a.png，512×480 + 右半 xOffset=512 的 128×480 切片拼成 640 宽）：
  script90 = sprite72 @(0,0) **层 22**（L23938-23947，`ins_6(170)` 后 60f 淡入）；
  script91 = sprite73 @(512,0) **层 21**（L23962-23968，同 170f+60f）。
  label：`3`=淡出 30f、`2`=淡入 60f、`1`=淡出销毁 60f（L23949-23957）。
- 红色星光 overlay（select00.png 512×480 + xOffset 512 切片）：script92=sprite74 @(0,0)
  层 21（L23986）、script93=sprite75 @(512,0) 层 21（L24022）。色 (64,64,64) 压暗
  （L23994）；label `2`=压暗淡入 170f 后隐藏（L23995-23999）、`3`=亮态：
  色→192 60f 后 **300f 交替 224↔192 呼吸，回跳 60 次共 61 轮**（L24006-24011）、
  `1`=淡出销毁（L24013-24014）。
- HANDOFF 称「60 帧呼吸」不准：60f 是入场调色，呼吸周期是 300f×2（上表以 dump 为准）。
- entry 结构：entry1=title00a（sprite72，L23903-23919）、entry2=title00a xOffset512
  （sprite73，L24059-24076）、entry3=select00（sprite74，L24080）、entry4=select00
  xOffset512（sprite75，L24100）。

### 1.6 LOGO：根 94/95 与五个子体（entry5 title_logo.png，L24121-24245）

- script94 首入场根（L24142）：`ins_6(110)` 延迟 110f → spawn 97（TEN DESIRES 翻转）→
  `ins_6(60)` 再延迟 → spawn 96+100（L24144-24148）。script95 复位根（L24155）：
  无延迟 spawn 99+96+100（自子菜单返回用，时机归属**待 oracle**）。
- script96 = sprite76（東方靈廟 256×192）@(132,150) 层 25，10f 淡入（L24166-24175）。
- script97 = sprite77（TEN DESIRES 256×256）**3D 翻转**：起 (448,368) scale2.5 旋转
  z=-π，60f 插值至 (132,150) scale1.0 旋转 0（ins_407/412/410，L24187-24192）；
  到位后 spawn 98 爆闪 + 恒慢速自转 `ins_415(0,0,0.009817477)`（L24194-24195）。
- script98 = 爆闪：alpha128→0 10f、scale→4.0、色→(64,64,255)（L24199-24213）。
- script99 = 静止慢转版（复位用，L24216-24227）。script100 = 副标题 sprite78（192×16）
  @(360,480) 层 25（L24230-24239），label 1 = 淡出 60f。

### 1.7 高亮条与光标（entry6 select01.png，L24248-24273）

- 高亮条 script101-110（L24277-24434）：sprite79-88（各 256×48），层 26，从 (-256,8)
  20f 滑入 **(32,8)**；label 1 = 滑回 -256 销毁。**注意**：脚本内位置固定 y=8、无根
  挂靠；但 `AnmManager::create` 支持逐实例 translation 覆盖（AnmManager.hpp:47），故
  (32,8) 仅为脚本默认值。HANDOFF 判定其属难度/角色菜单，与菜单行距 18px/行高不符
  （48px 高×10 条），**屏幕归属待锚定**（候选：PlayerData/结果屏行高亮）。
- 光标 script111/112（L24437/24455）：sprite89（14×14），层 25 alpha128，
  @(254,270)/@(382,270)，20f 放大至 scale(14,10)+alpha→208；label 1 = 缩灭销毁。
  一对左右位（x=254/382 同高 y=270）——HANDOFF 判定为难度/角色菜单光标，
  具体挂接屏**待 oracle**。

### 1.8 其余脚本组（如 dump 有内容，列出边界）

- script83-88（L23622-23902）：几何体（`ins_300(-1)` 回落精灵 + `ins_608/606` 屏幕四边形/
  模式19），扇形量表，label 6/7,17/8,18/9,19/10,20/1——为根 123/124 的子体（推定
  PlayerData/段位屏，**待锚定**）。
- script113-122（L24503-25247）：rank00.png 段位名条 sprite90-99（256×80，灰 113-117
  逐项 label 7..11/17..21/白 118-122）；根 123 @(320,178)（L25299，子 83,84,118-121,
  113-116）、根 124（L25322，子 83,84,117）——**屏幕归属待锚定**。
- 立绘 script125-128（L25418/25467/25516/25565）：sprite101-104（entry8-11 =
  face/pl00..pl03/title_pl0X.png，216×480/220×460/192×440/320×440），@(144,480) 层 23，
  anchor `ins_421(0,2)`。label（角色 c=0..3）：`7+c` 从右滑入（272→144，10f 淡入）、
  `12+c` 从左滑入（16→144）、`17+c` 向左滑出（层降 22、淡出）、`22+c` 向右滑出、
  `3` 中央 (160,480) 14f 淡出、`6` 恢复层 23、`1` 淡出销毁 20f。
- 角色菜单根 script129/130（L25614/25636）：129 spawn 125-128（立绘）+135-138（通常
  武器）+131-134（选择框）；130 同但 139-142（EX 武器）。129/130 分支条件
  （武器前后页？难度？）**待锚定**。
- 武器选择框 script131-134（L25658/25708/25758/25808）：sprite100（rank00.png 内
  128×96 块，L24499）@(64,128)/(80,96)/(96,128)/(96,96)，层 26 隐藏→25，3D 翻转
  （`ins_302(8)`+旋转 π→0）入场 20f；label `7+k/12+k/17+k/22+k/1`（k=0..3 逐项偏移）。
  用途（每角色一个框 or 每武器槽）**待锚定**。
- 武器图标 script135-138（通常，sprite105-108 400×256，L25938/26007/26076/26145）与
  139-142（EX，sprite109-112，L26294/26363/26432/26501）：@(416,240) 层 23，
  label `7+c` 从右（544→416，20f 3D 翻转）/`12+c` 从左（288→416）/`17+c` 出左/
  `22+c` 出右/`3` 淡出/`6` 层复位/`1` 销毁——与立绘同构的滑入体系。
- entry20 result00.png（L26571）+ script143-186（L26608-27796）= 结果屏；
  entry21 spell.png（L27851）+ script187-241（L27930-30481）= 符卡/练习屏。
  非标题三窗，仅登记边界，本轮不展开。

### 1.9 title_v.anm（bank17，dump L37812-37856）

仅 entry0 = title_ver.png 128×16（sprite0 126×14，L37828）+ **script0**（L37832）：
@(256,224)（L37836）层 26，alpha=0 起 30f 延迟淡入（L37840-37841）；label `3`=压暗 64、
`2`=回亮 255、`1`=10f 淡出销毁。**bank17 没有任何立绘**（HANDOFF 一致；现状代码把
bank17 当 portrait 用是病灶，见 §五-1）。

### 1.10 提示幕「按任意键」（ascii.anm=bank2，dump L15207-16314）

script19（L16282）：spawn 16×`ins_502(script18)` + 回跳 1 次（共 17 字符位循环，
L16290-16306），仅 label `1`（10f 淡出销毁，L16310-16311）。script18（L16242）=
变量驱动字符（ins_117 变量算术 + sprite304），**入场自带、无 label 2/3**——
th15 的 `interrupt(89,2/3)` 调用形状在 th13 不可用（标签缺失 → 该 VM 无动作）。
现状代码对提示幕 interrupt 2/3（TitleMainMenu.cpp:87/90/135/145）无效，须删。

## 二、TitleSceneResources 应改成的值（C++ 草案）

```cpp
// 证据：bank 表 Application.cpp:16-18；脚本号见 §一 各行号。
struct TitleSceneResources {
    i32 title = 16;    // title.anm
    i32 version = 17;  // title_v.anm（原 portrait=17 字段误当立绘，改名；§一.9）
    i32 ascii = 2;     // ascii.anm（TitleAnimations.hpp:10 默认 5 是坑，须显式传 2）
    i32 help = 19;     // help.anm（析构 retire_resource 用，§五-12）
    i32 main_root = 0;            // 主菜单根 (70,280)，子 3-22
    i32 difficulty_root = 1;      // 难度根 (70,312)，子 23-48
    i32 character_root = 2;       // 角色根 (70,312)，子 49-82
    i32 char_menu_root = 129;     // 角色菜单根（通常武器；130=EX，分支待锚定）
    i32 char_menu_root_ex = 130;
    i32 background_root = 89;     // 背景根（子 92,93,91,90）
    i32 logo_root = 94;           // LOGO 首入场根（内建 110f/60f 时序）
    i32 logo_restore_root = 95;   // LOGO 复位根（自子菜单返回）
    i32 version_bar = 0;          // title_v script0（bank=version）
    i32 prompt = 19;              // 提示幕 = ascii bank script19（≠title bank script3！）
    i32 portrait_base = 125;      // 立绘 125+角色号(0-3)，label 7/12/17/22+角色号
    i32 weapon_base = 135;        // 通常武器图标 135+角色号
    i32 weapon_base_ex = 139;     // EX 武器图标 139+角色号
    i32 weapon_selector_base = 131; // 武器选择框 131+k（用途待锚定）
};
```

派生常量：主菜单项数 `kTitleMainEntries = 10`（§1.2）；难度 5（§1.3）；角色 7（§1.4）。

## 三、TitleMainMenu::update 各 substate 创建/interrupt 序列

对照 th15 范式（th15 TitleMainMenu.cpp:20-76）逐点翻译；凡未标「实证」者为
**推定序列**，以 unicorn 标题窗 oracle 实测回写（§六）。

- **substate 0（进入/提示幕）**：
  - 若首次（flags 无 2）：`create(89)` 背景 + `create(94)` LOGO + `create(version,0)`
    版本条 + `create(ascii,19)` 提示幕 + 立绘 `create(title,125+char)` 后
    `interrupt(立绘, 7+char)`（从右滑入，方向推定）；`age.set(120)`。
    ① 时序细节（背景/LOGO/版本条建在按键前还是后）推定，oracle 锁定。
  - 若复位（自子菜单返回，flags 有 2）：`create(95)` 复位 LOGO（无翻转直达）+ 其余同上
    但立绘换 `interrupt(125+char, 12+char)` 或保持（推定）。
  - 现状病灶：`main_prompt=3` 建的是 title bank 菜单项 0（TitleMainMenu.cpp:86-91）、
    `portrait` 建的是版本条（:101-104）——全删。
- **substate 1（age==120 主菜单展开）**：`create(0)`（子 3-22 自动 spawn，白态 15f
  自淡入即未选中外观）+ `paint()`。现状 `paint()` 用 30/31（:43-49）在 th13 为空操作
  无害，可保留兼容形状。age>130 → substate 2 + `select_animation()`。
- **select_animation()（推定）**：`interrupt(0, 3, true)`（全体重置未选中）+
  `interrupt(0, 7+cursor, false)`（选中项灰态入场脉动）+ `paint()`。
  th15 同形（th15 :19）；th13 标签号恰好兼容——**这是本蓝图最重要的可原样迁移点**。
- **substate 2（交互）**：上下移动 → `sound(10)` + `select_animation()`；
  Esc/X → 跳到末项或直接 `change_substate(4)`+`sound(9)`（th15 :48-51 同形）；
  Z/Enter 确认 → `interrupt(0, 6, false)`（全项闪灭销毁，子脚本 label 6 实证存在）+
  `sound(7/9)` + 提示幕 `interrupt(19→1)` + 立绘 `interrupt(125+char, 3)`（中央淡出，
  推定）→ substate 4。
- **substate 4（age>19 分发）**：`enter(cursor)` 进 Difficulty/Quit 等；枚举补全见 §五-6。

## 四、TitleSelectionMenu 重锚

### 4.1 难度菜单（根 1，5 项+2 数字行）

- **substate 0**：`prompt()`（TitleAnimations.cpp:5，ascii19）；`retire(1)`→`create(1)`；
  `interrupt(1, 3, true)` 重置 + `interrupt(1, 7+cursor)` 选中。
  现状 `interrupt(root,3/cursor+13)`（TitleSelectionMenu.cpp:41-42）的标签算式是 th15
  值域：th13 难度子脚本只有 7-11/17-21（灰名逐项）、7/8（数字行）、3/7/17/27/28/29/2/6/1
  （白态与公共）——13/14 等号不存在（cursor≥4 时撞上灰名 0 的 17 与白态通用 17，
  行为错乱），**必须改成 3/7+cursor**。
- **substate 2**：移动 → `sound(10)` + `interrupt(1,3,true)` + `interrupt(1,7+new)`；
  Esc/X → `retire(1)`（根 label 1 实证存在，L17291）+ 回 Main；Z → `interrupt(1,6)`
  + `sound(7)` → substate 3。
- **substate 3（age>13）**：`choose_character()`。
- 数字行不单独调用（由中断传播联动，§〇-1）；若 oracle 显示需逐个 child_interrupt，
  按实测回写。光标 111/112、高亮 101-110 是否挂本屏：**待 oracle**。

### 4.2 角色菜单（根 2 + 根 129/130 立绘武器体系）

- **substate 0**：`create(2)` + `interrupt(2,3,true)` + `interrupt(2, 7+cursor)`（名条）；
  `create(129)`（或 130，分支待锚定）后：
  立绘 `child_interrupt(129, 125+cursor, 7+cursor)`（从右入，推定）+
  武器图标 `child_interrupt(129, 135+cursor, 7+cursor)` +
  选择框 `child_interrupt(129, 131+cursor, 7+cursor)`（若为逐角色框，推定）。
  现状 `interrupt(root,3/cursor+7)`（TitleSelectionMenu.cpp:114-115）形状可保留。
- **substate 2 左右移动**（现状 :126-135 用 th15 的 cursor+25/13/19/7）：改为
  立绘/武器滑动对：旧 `child_interrupt(129, 125+old, 17+old 或 22+old)`（出左/出右）+
  新 `child_interrupt(129, 125+new, 12+new 或 7+new)`（入左/入右），武器 135+同步；
  名条 `interrupt(2, 7+new)`。方向配对（左移=新从左入旧向右出？）**待 oracle**。
- **确认 Z**：现状 `selected_children` 用 `126/147/80/81`（:92-95）——126=立绘 pl01
  滑入脚本、147/80/81=结果屏脚本，th15 残留**全错**。应为：立绘
  `child_interrupt(129,125+cursor, 3)` + 武器 `child_interrupt(129,135+cursor, 3)`
  + 名条 `interrupt(2, 6)` + `sound(7)` → substate 3。
- **substate 3**：转场 effect bank8 script0（TitleScene.cpp:38-42）保留；age>39 开局
  （进度 character=cursor）。
- **substate 4**：`retire(129)` + 回难度屏。

## 五、TitleFrame / TitleState / TitleScene 差距清单（HANDOFF 13 项 → 代码落点）

| # | 差距 | 落点与修法 |
|---|---|---|
| 1 | portrait 用错 | TitleMainMenu.hpp:22 `portrait=17`→删；TitleMainMenu.cpp:101-104 `create(portrait,0)`→`create(16,125+char)`+interrupt 7+char（§三） |
| 2 | 背景缺失 | 全代码无 create(89)；在主菜单 substate 0 新增（§三），92/93 的 label 2/3 时机待 oracle |
| 3 | 提示幕脚本号错 | TitleMainMenu.hpp:24 `main_prompt=3`→ascii bank2 script19；TitleMainMenu.cpp:87/90/135/145 的 interrupt 2/3 删（§1.10） |
| 4 | LOGO 缺失 | 新增 create(94)/95（§1.6）；th15 无对应物，序列推定 |
| 5 | 版本号缺失 | 新增 `create(17,0)`；退场 interrupt 1（§1.9） |
| 6 | 菜单项数/枚举错 | TitleMainMenu.hpp:17-18 `kTitleMainEntries=5`→10；`TitleMainEntry` 枚举语义待辨认（位图文字）；TitleScreen 枚举值待 supervisor 锚定（见下） |
| 7 | 难度/角色脚本号未锚 | TitleSelectionMenu.cpp:41-42/55-56/63（难度）与 114-115/126-135（角色）的 th15 标签算式→§四 值；根自身无 3/6/7 标签（L17290/17337 仅 1），靠子传播（§〇-1）生效 |
| 8 | 角色选中态子脚本错 | TitleSelectionMenu.cpp:91-95 `126/147/80/81`→§4.2 确认序列 |
| 9 | TitleFrame 缺 demo/音乐重播/return_reason/Quit | TitleFrame.cpp:34-49 `initialize()` 只分 reason 0/其他；`music_age`（TitleFrame.hpp:40）未消费；demo/重播分支无；**待办**：需 supervisor 反汇编锚 return_reason 值域与 demo 计时（锚点待登记 AddressMap.hpp，候选 0x44e600 对应物） |
| 10 | TitleScene 缺屏 | TitleScene.cpp:49-61 仅 Main/Difficulty/Character；Quit 流（TitleFrame.cpp:62-67）已有雏形，Demo/音乐室等分期不做（ui-base.md §三） |
| 11 | Draw pass 占位 | TitleFrame.cpp:15-16 callbacks[1] 仅返回 enabled（TitleFrame.hpp:46 `drawing()`）；真实绘制回调体待主菜单对齐后接 ANM 渲染管线 |
| 12 | 析构漏 bank19 | TitleScene.cpp:20 只 retire {title,portrait}→补 `retire_resource(help=19)`（bank 常驻表 Application.cpp:18） |
| 13 | 测试用合成 ANM | tests/ui/ui_test_fixtures.hpp:32 `synthetic_anm`；主菜单落地后按本蓝图 §一 数值加真实 title.anm 脚本号回归（标签/锚点/层逐项断言） |

另：TitleState.hpp:14-15 `Quit=2` 与 `StartGame=2` **同值冲突**，TitleScreen 枚举整体
待锚（th15 世代值不可信）。

## 六、事实源排序声明

1. **oracle 真机行为 > 本蓝图**。unicorn 标题真机窗（另一工程师在建）实测的
   创建/中断序列若与本文任何「推定」条目冲突，以 oracle 为准，并**回写本文档**
   （改动处标注 `[oracle 修正 YYYY-MM-DD]`）。
2. 本蓝图非推定条目（脚本号/锚点/层/标签号/精灵尺寸/插值参数）均直接出自 dump，
   与 oracle 冲突时优先怀疑 dump 工具链（thtk 版本/解码），上报而不是擅自改数。
3. 位图内容（菜单项文字、数字组语义）只能由人类辨认 golden 帧或真机截图确认，
   禁止凭模型记忆断言（前科：Boss 名幻觉事故）。

## 七、验收门禁

1. 标题窗 / 难度窗 / 角色窗 draw_compare 均 `RESULT first_divergence_frame=none`
   （三窗各自独立 draws 窗口，覆盖矩阵原则）。
2. 对局窗回归 ≥ 分歧帧 511（不得回退；HANDOFF 已达成值）。
3. 每轮修复附带覆盖矩阵更新：本轮 oracle 覆盖（三窗）/ 未覆盖（Quit/Demo/音乐室等
   分期屏）清单交接下一阶段。

## 附：待锚定清单汇总（消费顺序即优先级）

1. 主菜单 10 项 / 难度 5 项 / 角色 7 项的位图文字语义（人类辨认 golden 帧）。
2. 主菜单选中切换的准确标签组合（3+7+k 推定 vs 27/28/29 个体微调）——oracle 锁定。
3. 难度/角色数字组的数值语义与动态换字机制——oracle + supervisor。
4. 角色 7 项构成（4 角色+3 项其他?）、数字行仅 0-4 的对应关系。
5. 根 129/130 分支条件（通常/EX 武器页）。
6. 高亮 101-110 与光标 111/112 的屏幕归属（HANDOFF 判定存疑，§1.7）。
7. LOGO 94/95 的启用时机（首入 vs 返回）；背景 92/93 label 2/3 的调用点。
8. TitleScreen 枚举真值、return_reason 值域、demo 计时（需 supervisor 反汇编，
   锚点待登记 `web/cpp/rebuild/AddressMap.hpp`）。
9. 菜单项禁用语义（Replay/PlayerData/MusicRoom 分期禁用后灰态处理）——与真机
   10 项全亮不同，需在覆盖矩阵登记为「有意偏差」或补齐。
