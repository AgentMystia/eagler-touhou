// 原版 th16.exe（v1.00a，PE 时间戳 2017-07-31，SHA-256 c1177601…1929）地址锚点表。
// 每完成一个子系统的行为还原，就在此登记它的原版入口与 C++ 对应物，
// 使 oracle 对照与回归脚本可以直接引用，不再散落硬编码地址。
// 反编译证据：artifacts/th16/decomp/jp/functions/<地址>.{c,asm}（工作区忽略目录）。
#pragma once

#include <cstdint>

namespace th16::rebuild {

constexpr uint32_t kImageBase = 0x400000;
constexpr uint32_t kEntryPoint = 0x47537B;  // PE 头入口（CRT 启动）
constexpr uint32_t kCrtBoundary = 0x470000;  // 以上为静态链接 MSVC CRT（1006 游戏函数 / 755 CRT 函数）

struct AddressAnchor {
    const char* subsystem;
    uint32_t originalEntry;
    const char* cpp;  // C++ 对应物（nullptr = 未还原）
};

constexpr AddressAnchor kAnchors[] = {
    // —— 已还原（本轮）——
    {"rng-step16", 0x402B70, "game/Rng.hpp Rng::next16"},
    {"rng-step32", 0x402BE0, "game/Rng.hpp Rng::next32"},
    {"rng-unit", 0x402C70, "game/Rng.hpp Rng::unit"},
    {"rng-signed-unit", 0x402CB0, "game/Rng.hpp Rng::signed_unit"},
    {"rng-angle", 0x402CF0, "game/Rng.hpp Rng::angle"},
    {"resource-crypt", 0x402220, "game/ResourceCrypt.cpp resource_crypt"},
    {"archive-open", 0x4572E0, "game/Archive.cpp Archive::open"},
    {"archive-directory", 0x4574B0, "game/Archive.cpp Archive::open（目录项）"},
    {"archive-read-entry", 0x457120, "game/Archive.cpp Archive::read"},
    {"archive-lzss", 0x457F00, "game/Lzss.cpp Lzss::decode"},
    {"anm-bank-load", 0x46CDD0, "game/AnmResource.cpp AnmResource::parse（格式面）"},
    {"anm-entry-check", 0x46D0C0, "game/AnmResource.cpp（version==8 / hasdata / '@'）"},
    {"anm-bank-slot", 0x46CF80, "game/AnmManager.cpp AnmManager::load"},  // 31 个 bank 槽（param_1 ≤ 0x1e），对象 0x13c 字节
    {"anm-loader-step", 0x46D1C0, "game/AnmManager.cpp AnmManager::load（精灵表 / 模板预跑）"},
    {"anm-sprite-table", 0x46D3B0, "game/AnmManager.cpp AnmManager::load（0x44 精灵记录，'@R' 用后备缓冲尺寸）"},
    {"anm-sprite-normalize", 0x46D8A0, "game/AnmManager.cpp AnmManager::load（uv / ratio）"},
    {"anm-manager-ctor", 0x46A3A0, "game/AnmManager.cpp AnmManager::AnmManager（0x1fff VM 池）"},
    {"anm-vm-update", 0x45F980, "game/AnmVm.cpp AnmInterpreter::run"},   // 13109 字节、64 个调用方：ANM VM 指令解释
    {"anm-vm-reset", 0x4090F0, "game/AnmManager.cpp reset_vm"},
    {"anm-vm-reset-tail", 0x407A50, "game/AnmManager.cpp reset_tail"},
    {"anm-template-init", 0x45F020, "game/AnmManager.cpp init_template"},
    {"anm-template-copy", 0x407B20, "game/AnmManager.cpp AnmManager::init_from_template"},
    {"anm-set-sprite", 0x45EDB0, "game/AnmVm.cpp AnmInterpreter::set_sprite"},
    {"anm-int-var", 0x45F610, "game/AnmVm.cpp AnmInterpreter::int_var"},
    {"anm-float-var", 0x45F2D0, "game/AnmVm.cpp AnmInterpreter::float_var"},
    {"anm-interp-step", 0x463B30, "game/AnmVm.cpp AnmInterpreter::step_interps / step_interp / step_interp_angle"},
    {"anm-ease", 0x4033F0, "game/AnmVm.cpp ease（31 种缓动）"},
    {"anm-velocity", 0x4630F0, "game/AnmVm.cpp AnmInterpreter::apply_velocities"},
    {"timer-tick", 0x406190, "game/AnmVm.cpp Timer::tick"},
    {"timer-set", 0x406490, "game/AnmVm.hpp Timer::set"},
    {"timer-untick", 0x40D490, "game/AnmVm.cpp Timer::untick"},
    {"anm-spawn", 0x406380, "game/AnmManager.cpp AnmManager::spawn"},
    {"anm-spawn-child", 0x46ED60, "game/AnmManager.cpp AnmManager::create_child（ins_500–506）"},
    {"anm-spawn-detached", 0x46EEA0, "game/AnmManager.cpp AnmManager::create_detached（ins_507）"},
    {"anm-pool-alloc", 0x46F600, "game/AnmManager.cpp AnmManager::allocate"},
    {"anm-pool-free", 0x46EAB0, "game/AnmManager.cpp AnmManager::release"},
    {"anm-mark-deleted", 0x46E660, "game/AnmManager.cpp AnmManager::mark_deleted"},
    {"anm-world-insert-back", 0x46E7D0, "game/AnmManager.cpp AnmManager::insert_world_back"},
    {"anm-world-insert-front", 0x46E890, "game/AnmManager.cpp AnmManager::insert_world_front"},
    {"anm-ui-insert-back", 0x46E940, "game/AnmManager.cpp AnmManager::insert_ui_back"},
    {"anm-ui-insert-front", 0x46EA00, "game/AnmManager.cpp AnmManager::insert_ui_front"},
    {"anm-tick-world", 0x46E340, "game/AnmManager.cpp AnmManager::tick_world"},
    {"anm-tick-ui", 0x46E490, "game/AnmManager.cpp AnmManager::tick_ui"},
    {"anm-find", 0x46EFA0, "game/AnmManager.cpp AnmManager::find"},
    {"ascii-manager-init", 0x407CB0, "game/AnmManager.hpp kAsciiSlot（ascii*.anm → 槽 2；ins_300 负下标回退精灵 0x102）"},
    {"anm-draw-vm", 0x468490, "game/AnmRenderer.cpp AnmRenderer::emit（mode 0..3 屏幕四边形 + mode 8；其余模式待扩展）"},
    {"anm-draw-projected", 0x467410, "game/AnmRenderer.cpp AnmRenderer::emit_projected（mode 8：世界矩阵 / 欧拉顺序 / 纹理矩阵 / TFACTOR / Z 写）"},
    {"anm-unit-quad-vb", 0x46B900, "game/AnmRenderer.cpp emit_projected（±128 单位四边形 × 9 锚点变体）"},
    {"anm-world-rotation", 0x464DD0, "game/AnmVm.cpp anm_world_rotation"},
    {"anm-render-state", 0x464F10, "game/AnmRenderer.cpp address_mode + sdl/Application.cpp apply_blend（混合 / 采样 / 寻址）"},
    {"device-default-state", 0x45BA80, "sdl/Application.cpp draw_frame（ZENABLE+ZFUNC ALWAYS、alpha test ref 1、调制）"},
    {"camera-setup", 0x43C780, "game/AnmRenderer.cpp AnmCamera::setup（LookAtLH / PerspectiveFovLH）"},
    {"camera-ui-init", 0x43CB10, "game/AnmRenderer.hpp AnmCamera 缺省值（0x4c1560：fov π/6、全窗口视口）"},
    {"camera-apply", 0x43C710, "game/AnmRenderer.cpp AnmCamera::project（view/proj → 视口）"},
    {"anm-draw-layer", 0x46E750, "game/AnmRenderer.cpp AnmRenderer::emit_layer"},
    {"anm-quad-axis", 0x465C40, "game/AnmRenderer.cpp AnmRenderer::axis_corners"},
    {"anm-quad-rotated", 0x4660B0, "game/AnmRenderer.cpp AnmRenderer::rotated_corners"},
    {"anm-quad-submit", 0x465280, "game/AnmRenderer.cpp AnmRenderer::emit（像素对齐 / UV 滚动 / 颜色）"},
    {"anm-world-pos", 0x406C40, "game/AnmRenderer.cpp AnmRenderer::world_position"},
    {"anm-world-adjust", 0x406A70, "game/AnmRenderer.cpp AnmRenderer::transform_position（窗口缩放 / root 链 / 父旋转 / layer group 偏移）"},
    {"boot-scene-init", 0x43ADC0, "game/GameSession.cpp GameSession::begin_boot（sig.anm → 槽 1，text.anm → 槽 0，注册启动幕绘制链 0x44）"},
    {"front-load", 0x43AD20, "game/GameSession.cpp GameSession::begin_boot（front.anm → 槽 5）"},
    {"effect-banks-load", 0x418790, "game/GameSession.cpp GameSession::begin_boot（effect.anm → 槽 8，bullet.anm → 槽 7）"},
    {"boot-scene-draw", 0x43B300, "game/GameSession.cpp begin_boot 内绘制链回调（spawn / boot_frames++）"},
    {"boot-scene-update", 0x43B290, "game/Supervisor.hpp Supervisor::boot_init_finished（请求 Title=4）"},
    {"supervisor-register", 0x43BA40, "game/Supervisor.hpp（主更新 0x43b3d0 / 场景切换 0x43ce10）"},
    {"supervisor-frame", 0x43B3D0, "game/GameSession.cpp GameSession::logic_step"},
    {"supervisor-switch", 0x43CE10, "game/Supervisor.hpp Supervisor::consume_switch"},
    {"chain-alloc", 0x401730, "game/Chain.cpp ChainManager::create"},
    {"update-chain-insert", 0x401300, "game/Chain.cpp ChainManager::insert_sorted（含 init 回调）"},
    {"draw-chain-insert", 0x4013B0, "game/Chain.cpp ChainManager::insert_sorted"},
    {"update-chain-walk", 0x401460, "game/Chain.cpp ChainManager::run_update（0=Remove 3/4/5=Break 6=Restart）"},
    {"draw-chain-walk", 0x4015A0, "game/Chain.cpp ChainManager::run_draw"},
    {"chain-remove", 0x4017A0, "game/Chain.cpp ChainManager::remove"},
    {"frame-clock", 0x45A8A0, "game/FrameClock.hpp FrameClock::begin_frame（1/60 累加；浏览器按 rAF 折算步数）"},
    {"input-sample", 0x401D50, "game/Input.hpp InputState 位布局 + sdl/Application.cpp sample_input（键盘位收集）"},
    {"input-edges", 0x405580, "game/Input.hpp InputState::commit"},
    {"title-scene-create", 0x44AEE0, "game/GameSession.cpp GameSession::enter_title_assets"},
    {"title-scene-thread", 0x44ABC0, "game/GameSession.cpp GameSession::activate_title_menu（等 0xb4 帧 / DAT_004c1800&0x180）"},
    {"title-scene-init", 0x44AC70, "game/GameSession.cpp enter_title_assets（title.anm / title_v.anm）+ 更新链 6 / 绘制链 0x45"},
    {"title-update", 0x44AF80, "game/TitleScene.cpp TitleScene::update（画面分发；末尾 Timer+1）"},
    {"title-main-menu", 0x44B5F0, "game/TitleScene.cpp TitleScene::update_main_menu（case 0/1/2/4）"},
    {"title-difficulty", 0x44FE20, "game/TitleScene.cpp TitleScene::update_difficulty"},
    {"title-shot", 0x4502C0, "game/TitleScene.cpp TitleScene::update_shot"},
    {"title-season", 0x450AF0, "game/TitleScene.cpp TitleScene::update_season（case 3 请求场景 7）"},
    {"title-stage", 0x450EF0, "game/TitleScene.cpp TitleScene::update_stage（练习关卡）"},
    {"title-item-interrupt-run", 0x44A700, "game/TitleScene.cpp TitleScene::item_interrupt_now"},
    {"title-item-interrupt", 0x44A680, "game/TitleScene.cpp TitleScene::item_interrupt"},
    {"title-child-id", 0x44A790, "game/TitleScene.cpp TitleScene::child_id"},
    {"title-dismiss", 0x44A650, "game/TitleScene.cpp TitleScene::dismiss"},
    {"title-set-screen", 0x44A560, "game/TitleScene.hpp TitleScene::set_screen"},
    {"title-set-sub", 0x44A5E0, "game/TitleScene.hpp TitleScene::set_sub"},
    {"game-set-mode", 0x449190, "game/GameGlobals.hpp GameGlobals::set_mode"},
    {"anm-reveal", 0x46F340, "game/TitleScene.cpp TitleScene::reveal（0x46f3b0 递归）"},
    {"title-leave", 0x44AF50, "game/GameSession.cpp GameSession::leave_title"},
    {"title-destroy", 0x44AD20, "game/GameSession.cpp GameSession::leave_title（移除链 / 卸载 0x10、0x11 / 落叶发射器 +0x73c interrupt 1）"},
    {"menu-cursor-move", 0x402E70, "game/MenuCursor.hpp MenuCursor::move"},
    {"menu-cursor-set", 0x418720, "game/MenuCursor.hpp MenuCursor::set"},
    {"menu-cursor-disable", 0x440C00, "game/MenuCursor.hpp MenuCursor::disable"},
    {"menu-cursor-push", 0x402DE0, "game/MenuCursor.hpp MenuCursor::push"},
    {"menu-cursor-pop", 0x402E20, "game/MenuCursor.hpp MenuCursor::pop"},
    {"menu-edge-or-repeat", 0x4186F0, "game/TitleScene.cpp（pressed | repeat 掩码判定）"},
    {"timer-set", 0x406490, "game/TitleScene.cpp（Timer=0x78 直接赋值）"},
    {"anm-find-child", 0x46F510, "game/AnmManager.cpp AnmManager::find_child"},
    {"anm-interrupt", 0x46F0B0, "game/AnmManager.cpp AnmManager::interrupt"},
    {"anm-interrupt-run", 0x46F130, "game/AnmManager.cpp AnmManager::interrupt_now"},
    {"anm-kill-file", 0x46F270, "game/AnmManager.cpp AnmManager::kill_file"},
    {"player-create", 0x441C60, "game/GameSession.cpp GameSession::create_player + game/Player.cpp Player::init"},
    {"player-init", 0x440FB0, "game/Player.cpp Player::init（pl0N.anm 槽 9 / pl0Nsub.anm 槽 0x1e、.sht、更新链 0x16 / 绘制链 0x1d）"},
    {"player-set-pos", 0x440E40, "game/Player.cpp Player::set_position"},
    {"player-reset", 0x441740, "game/Player.cpp Player::reset（0x42dc50 首帧）"},
    {"player-options-interrupt", 0x440DC0, "game/Player.cpp Player::interrupt_options"},
    {"player-update", 0x442560, "game/Player.cpp Player::update（0x443720 更新链 0x16 入口）"},
    {"player-move", 0x441CF0, "game/Player.cpp Player::move"},
    {"player-options-tick", 0x442380, "game/Player.cpp Player::update_options"},
    {"player-options-setup", 0x4440E0, "game/Player.cpp Player::setup_options"},
    {"player-shoot", 0x4455D0, "game/Player.cpp Player::shoot（射击门限与两计时；0x440d50 start_shot_timer）"},
    {"player-fire", 0x445470, "game/Player.cpp Player::fire_sets（.sht 组 = 火力/每级 (+1+最大等级 低速)，period/phase 命中出弹；子机组 = 季节子机数）"},
    {"player-fire-slot", 0x445360, "game/Player.cpp Player::fire_sets（类型 2 冷却表 +0x165f8[k+sub*8]、首个空弹位）"},
    {"player-bullet-create", 0x444E10, "game/Player.cpp Player::spawn_bullet（角度环绕 <995 / ≥995 子机角 / ≥1000 随机 ±π/12；VM 脚本 (+5 子机)；伤害源 + 生成回调）"},
    {"player-bullet-create-cb", 0x445ED0, "game/Player.cpp Player::spawn_bullet（回调 1：target=0；回调 4 0x447450：伤害源类型 2）"},
    {"player-bullet-tick-cb", 0x445EE0, "game/Player.cpp Player::tick_bullets（回调 1 追踪：0x425240 找 256 内最近敌机转向加减速；回调 5 0x447480：active==1 时 speed+=0.5）"},
    {"player-homing-target", 0x425240, "game/Player.cpp Player::tick_bullets（在 (flags&0xc000021)==0 的敌机中找距离 <256 且最近者）"},
    {"player-bullets", 0x4456D0, "game/Player.cpp Player::tick_bullets（回调→运动→VM；in-bounds = 类型 2 || 计时<0xf || 四角任一严格在 (0,0)-(384,448) 内；否则 kill VM 并回收）"},
    {"damage-source-alloc", 0x444B20, "game/Player.cpp Player::alloc_damage_source（从上次下标后首个空位，1 起编号；类型 1、f7c=10000000、计时 9999999）"},
    {"damage-source-apply", 0x445A30, "game/Player.cpp Player::apply_damage_sources（0x403d30 旋转矩形-圆相交、f84 防同帧重入、按 kind 分派命中回调、sht.damage_cap 封顶）"},
    {"rect-circle-hit", 0x403D30, "game/Player.cpp Player::apply_damage_sources rect_circle"},
    {"damage-hit-bullet", 0x445D40, "game/Player.cpp Player::apply_damage_sources（kind 1：按 .sht hit_callback 分派 → 0x445e20）"},
    {"bullet-hit-default", 0x445E20, "game/Player.cpp Player::hit_bullet_default（z=0.1、VM interrupt 1、active=2、speed*=0.125、销毁伤害源）"},
    {"bullet-hit-aya", 0x447270, "game/Player.cpp Player::apply_damage_sources（hit_callback 5：effect.anm 0x98 + 0x445e20）"},
    {"damage-hit-aya-charge", 0x4474A0, "game/Player.cpp Player::apply_damage_sources（kind 2：返回原始伤害并将弹伤害置 1 持续穿透）"},
    {"anm-spawn-effect", 0x40E5C0, "game/AnmManager.cpp AnmManager::spawn_effect（首帧 run 前设 entity_offset 与 rotation.z）"},
    {"motion-velocity", 0x402FF0, "game/Motion.cpp MotionState::update_velocity（模式 0 极坐标 / 2 半径角度；3/4 敌弹时补）"},
    {"motion-integrate", 0x403110, "game/Motion.cpp MotionState::integrate（坐标 floor 到 1/100）"},
    {"vm-corners", 0x468C00, "game/AnmRenderer.cpp AnmRenderer::vm_corners（模式 0/2/3 轴对齐 / 1 旋转 + world_position）"},
    {"sfx-play", 0x45E1F0, "game/Player.cpp sfx_queue → GameSession::sfx_events（Sound 管理器待接；RET 4）"},
    {"player-draw", 0x443730, "game/Player.cpp Player::prepare_draw（0x468490 直绘待接）"},
    {"sht-load", 0x443790, "game/ShotTable.cpp ShotTable::parse"},
    {"season-step", 0x43DEB0, "game/GameGlobals.hpp GameGlobals::set_season_step"},
    {"anm-spawn-front", 0x426160, "game/AnmManager.cpp AnmManager::spawn_front"},
    {"anm-kill", 0x46F1C0, "game/AnmManager.cpp AnmManager::kill"},
    {"anm-kill-tree", 0x46F220, "game/AnmManager.cpp AnmManager::kill_tree"},
    {"timer-rewind", 0x43AC80, "game/AnmVm.cpp Timer::rewind"},
    {"replay-record", 0x447FD0, "game/GameSession.cpp create_player 记录链 0x10（DAT_004a52c8 = held 低 16 位）"},
    {"game-input-edges", 0x418650, "game/Input.hpp InputState::commit（DAT_004a52c8 族）"},
    {"anm-unload-file", 0x46D770, "game/AnmManager.cpp AnmManager::unload"},
    {"game-manager-create", 0x42D700, "game/GameSession.cpp GameSession::enter_game（DAT_004a6dd4；更新链 0xf / 绘制链 2；装载线程 0x42d1f0→0x42cb60）"},
    {"game-manager-load", 0x42CB60, "game/GameSession.cpp enter_game（Player + StageManager + EnemyManager/ECL；Gui/Bullet/Item/Effect 待还原）"},
    {"game-manager-update", 0x42D7B0, "game/GameManager.cpp GameManager::update（timer 0 → first_frame；DAT_004a579c/DAT_004a57a0++）"},
    {"game-manager-first-frame", 0x42DC50, "game/GameManager.cpp GameManager::first_frame（0x40add0 启动关卡面 VM、0x441740 自机复位、0x41aa70 生成 main）"},
    {"game-manager-destroy", 0x42D200, "game/GameSession.cpp GameSession::leave_game"},
    {"score-rng-init", 0x449720, "game/GameSession.cpp load_boot_banks（0x4497e0 ScoreFile 构造：DAT_004a6d88 做 0x1ee 次 next16）"},
    {"replay-rng-reset", 0x449030, "game/GameSession.cpp enter_game（0x449e50 Replay 构造：重置 DAT_004a6d8c 调用计数）"},
    {"ecl-load", 0x474530, "game/Ecl.cpp EclResource::open + EclProgram::attach（SCPT 头、ANIM/ECLI 包含、ECLH 子程序表排序）"},
    {"ecl-find-sub", 0x474740, "game/Ecl.cpp EclProgram::find_index（二分查找合并子程序表）"},
    {"ecl-step", 0x472030, "game/Ecl.cpp EclContext::step（通用 ECL 虚拟机操作码 0..0x5d、栈帧、表达式栈、插值槽）"},
    {"ecl-call", 0x471DB0, "game/Ecl.cpp EclContext::call_into / return_from_call（调用帧参数类型转换与恢复）"},
    {"ecl-spawn-async", 0x474430, "game/Enemy.cpp Enemy::spawn（op 15/16 异步子线程）"},
    {"ecl-arg-int", 0x473E40, "game/Ecl.cpp EclContext::integer（字面量 / 局部帧 / 表达式栈 / 宿主全局变量）"},
    {"ecl-arg-float", 0x473EF0, "game/Ecl.cpp EclContext::floating"},
    {"enemy-manager-create", 0x41B340, "game/Enemy.cpp EnemyManager::create + GameSession.cpp enter_game（0x41ae70 装 ECL + 登记更新链 0x1a）"},
    {"enemy-manager-update", 0x41B3D0, "game/Enemy.cpp EnemyManager::update（0x41b4f0 入口：遍历敌人链表、清 0x40000 重入位、同步 enemy_count）"},
    {"enemy-spawn", 0x41AA70, "game/Enemy.cpp EnemyManager::spawn_enemy / Enemy::init（分配 id、flags、invuln=2、life_timer=0x3c，立即执行首帧 0x41d1e0）"},
    {"enemy-update", 0x41D2E0, "game/Enemy.cpp Enemy::update（0x41d1e0：0x40000 隔帧门控 → 运动 0x41bb50 → ECL 0x473bc0 → 碰撞 0x41c330 → invuln/season_drop 倒数）"},
    {"enemy-motion", 0x41BB50, "game/Enemy.cpp Enemy::update（角度/速度/位置插值 → motion1/2 → 0x41c1f0 合成与 0x20000 边界钳制 → 精灵半宽高场地进出判定）"},
    {"enemy-motion-combine", 0x41C1F0, "game/Enemy.cpp Enemy::update（pos += (m1+m2-pos) floor 1/100 + 0x20000 边界框钳制）"},
    {"enemy-collision", 0x41C330, "game/Enemy.cpp Enemy::update（0x445a30 伤害源结算、0x41a6d0 扣血、0x41d520 死亡与 0x41d700 季节掉落 RNG、受击闪烁与音效）"},
    {"enemy-sub-life", 0x41A6D0, "game/Enemy.cpp Enemy::update（普通扣血与符卡 life_flags&1 下 7 倍减伤池折算）"},
    {"enemy-death", 0x41D520, "game/Enemy.cpp Enemy::update（死亡音效 (id&1)+3、限时季节掉落数线性插值、0x41d700 掉落 RNG）"},
    {"enemy-drop-items", 0x41D700, "game/Enemy.cpp Enemy::update（季节道具掉落消耗 visual_rng）"},
    {"enemy-ecl-dispatch", 0x41DCB0, "game/Enemy.cpp Enemy::execute（敌方操作码 ≥300：生成 300..305、ANM 302/303/306、运动 400/401/404..407/412、属性 500..505/511..515/520/522/535..539/548/568、掉落 1000）"},
    {"enemy-spawn-op", 0x423050, "game/Enemy.cpp Enemy::execute case 300..305（相对/绝对/镜像生成子敌人）"},
    {"enemy-var-get", 0x423810, "game/Enemy.cpp Enemy::integer / Enemy::floating（-10000..-9902 变量读取）"},
    {"enemy-var-int-dst", 0x423F80, "game/Enemy.cpp Enemy::integer_destination"},
    {"enemy-var-float-dst", 0x424C10, "game/Enemy.cpp Enemy::float_destination"},
    {"stage-table", 0x4A22D0, "game/StageTable.hpp kStageTable（8 × 0xd4：std/ecl/bgm/msg/logo/曲号）"},
    {"stage-manager-create", 0x409DB0, "game/Stage.cpp StageManager::load（DAT_004a6d9c/a0；更新链 0x11 / 绘制链 3、6）"},
    {"stage-manager-ctor", 0x4097C0, "game/Stage.cpp StageManager::load（相机默认 pos (0,0,-600) facing (0,300,600) up (0,1,0)）"},
    {"std-load", 0x40AC30, "game/Std.cpp StdFile::parse + Stage.cpp load（背景 ANM → 槽 3 + (stage & 1)）"},
    {"std-start", 0x40ADD0, "game/Stage.cpp StageManager::start（面 VM init_from_template + 首跑；vm_index 顺序分配）"},
    {"stage-update", 0x409E50, "game/Stage.cpp StageManager::update（脚本 → 摇晃 → 面 VM → 层精灵 → 相机矩阵 0x43c940）"},
    {"stage-script", 0x40B3B0, "game/Stage.cpp StageManager::run_script / apply_rocking（param_1 = this+0xc；跳转表 0x40bfbc，0xf/0x10 为空操作）"},
    {"stage-faces-step", 0x40AED0, "game/Stage.cpp StageManager::step_faces（全部 ins_offset<0 时清对象 bit0）"},
    {"stage-interrupt", 0x40B2F0, "game/Stage.cpp StageManager::interrupt（ins 19：arg+7 → 全部面/层 VM）"},
    {"stage-draw", 0x409F90, "game/AnmRenderer.cpp AnmRenderer::emit_stage（绘制链 3：清场地矩形、相机 0x4c168c、逐层 0..7）"},
    {"stage-draw-layer", 0x40AF70, "game/Stage.cpp StageManager::draw_layer（实例 × 四边形；entity_offset / 尺寸缩放；雾 mode 8/0x18；ZENABLE=!(lo&0x2000)）"},
    {"stage-draw-layer-sprites", 0x40C280, "game/Stage.cpp StageManager::draw_layer（层精灵 8 个，无雾、Z 关）"},
    {"camera-matrices", 0x43C940, "game/AnmRenderer.cpp AnmCamera::look_at（LookAtLH + PerspectiveFovLH zn=30 zf=8000）"},
    {"fog-color-add", 0x40D370, "game/Stage.cpp fog_add（BGRA 浮点分量相加）"},
    // —— 已定位，待还原 ——
    {"stage-draw-6", 0x40A410, nullptr},     // 绘制链 6：ANM 层 0x20/0x21 用 3D 相机
    {"stage-frustum-cull", 0x40A7D0, nullptr}, // 对象包围盒视锥剔除（当前全部视为可见，只影响实例 bit0）
    {"stage-distortion", 0x40C4A0, nullptr}, // ins 17 畸变效果（+0x3314）
    {"stage-effect-fade", 0x43D140, nullptr}, // 绘制链 1：清后台缓冲、RT DAT_004c127c、场地矩形清色 DAT_004c1b0c
    {"rt-composite", 0x43D2F0, nullptr},     // 绘制链 0xf：RT 精灵 VM 0x4c128c 合成到后台缓冲（当前以视口裁剪 + x−96 代替）
    {"player-body-collide", 0x4439E0, nullptr}, // 敌机与自机身体体术碰撞（0x41c330 下半段）
    {"effect-attach", 0x418AF0, nullptr},    // ins_508：EffectManager（DAT_004a6db8）按效果种类表 DAT_004a2250 挂 update/draw 回调（effect.anm 155–196 依赖）
    {"effect-ring-update", 0x405700, nullptr}, // 效果种类 1 的 update 回调（DAT_004919e8[2]）：环形顶点缓冲 + 生成 effect.anm 0x99
    {"anm-load-file", 0x46D020, nullptr},    // AnmManager::load_file(slot, name)：从档案读 .anm 并进 bank（同步等待装载线程）
    {"anm-draw-mode4+", 0x466390, nullptr},  // mode 4+（投影 / 世界 / 条带 / 网格）：0x466390 / 0x466820 / 0x4671b0 / …
    {"title-has-clear", 0x44A850, nullptr},  // 任一难度通关记录 → Extra 可选（当前恒 0：无存档）
    {"title-difficulty-cleared", 0x44A8E0, nullptr}, // 某难度四项记录齐全（难度面板 0x9e+d 标记）
    {"title-extra-unlocked", 0x44A800, nullptr}, // 机体是否解锁 Extra
    {"title-demo", 0x44AF80, nullptr},       // 0x44af80 开头：无输入 0x708 帧后播放 demo 录像（DAT_004a5bf0）
    {"title-loading-text", 0x41A390, nullptr},  // ascii.anm 0x11（季节确定后 Timer==10）
    {"effect-spawn-transition", 0x418BA0, nullptr}, // EffectManager 种类 0 过场（DAT_004c12a0，interrupt 7）
    {"bgm-fade", 0x43C470, nullptr},         // 0x45ed00(…,5,…)：BGM 淡出
    {"title-option", 0x44C570, nullptr},     // 画面 3：Option
    {"title-replay", 0x451750, nullptr},     // 画面 0xb
    {"title-score", 0x452330, nullptr},      // 画面 0xa：Player Data
    {"title-music-room", 0x4546F0, nullptr}, // 画面 0xd
    {"title-manual", 0x4545A0, nullptr},     // 画面 0x10
    {"title-spell-practice", 0x4553D0, nullptr}, // 画面 0x11
    {"title-draw", 0x44B530, nullptr},       // 标题绘制链 0x45
    {"score-file", 0x4497E0, nullptr},       // scoreth16.dat 读写
    {"replay-file", 0x4403C5, nullptr},      // th16_%.2d.rpy
    {"ver-file", 0x43B480, nullptr},         // th16_%.4x%c.ver
    {"music-fmt", 0x43AEAE, nullptr},        // ../../bgm/thbgm.fmt
    // —— 未定位 ——
    {"bullet-manager", 0, nullptr},
    {"item-manager", 0, nullptr},
    {"player-laser-create", 0x446200, nullptr},  // 魔理沙激光（回调 2：0x446260 / 0x446870）
    {"player-death", 0x443CD0, nullptr},     // 被弹结算（状态 4 → 2）
    {"season-system", 0, nullptr},  // th16 特有：季节槽/季节解放
};

}  // namespace th16::rebuild
