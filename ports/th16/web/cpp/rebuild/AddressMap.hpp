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
    {"anm-draw-vm", 0x468490, "game/AnmRenderer.cpp AnmRenderer::emit（mode 0..3 屏幕四边形；更高模式待扩展）"},
    {"anm-draw-layer", 0x46E750, "game/AnmRenderer.cpp AnmRenderer::emit_layer"},
    {"anm-quad-axis", 0x465C40, "game/AnmRenderer.cpp AnmRenderer::axis_corners"},
    {"anm-quad-rotated", 0x4660B0, "game/AnmRenderer.cpp AnmRenderer::rotated_corners"},
    {"anm-quad-submit", 0x465280, "game/AnmRenderer.cpp AnmRenderer::emit（像素对齐 / UV 滚动 / 颜色）"},
    {"anm-world-pos", 0x406C40, "game/AnmRenderer.cpp AnmRenderer::world_position"},
    {"anm-world-adjust", 0x406A70, "game/AnmRenderer.cpp AnmRenderer::world_position（窗口缩放 / root 链）"},
    {"boot-scene-init", 0x43ADC0, "game/GameSession.cpp GameSession::begin_boot（sig.anm → 槽 1，注册启动幕绘制链 0x44）"},
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
    {"title-update", 0x44AF80, "game/TitleMenu.hpp TitleMenu::update（case 0/1 主菜单入口；末尾 Timer+1）"},
    {"title-main-menu", 0x44B5F0, "game/TitleMenu.hpp TitleMenu::update（case 0/1/2/4：装饰 / 光标 / 确定取消 / 画面跳转）"},
    {"title-item-interrupt-run", 0x44A700, "game/TitleMenu.hpp TitleMenu::item_interrupt_now"},
    {"title-item-interrupt", 0x44A680, "game/TitleMenu.hpp TitleMenu::item_interrupt"},
    {"title-set-screen", 0x44A560, "game/TitleMenu.hpp TitleMenu::Screen / next_screen"},
    {"title-set-sub", 0x44A5E0, "game/TitleMenu.hpp TitleMenu::set_sub"},
    {"menu-cursor-move", 0x402E70, "game/MenuCursor.hpp MenuCursor::move"},
    {"menu-cursor-set", 0x418720, "game/MenuCursor.hpp MenuCursor::set"},
    {"menu-cursor-disable", 0x440C00, "game/MenuCursor.hpp MenuCursor::disable"},
    {"menu-cursor-push", 0x402DE0, "game/MenuCursor.hpp MenuCursor::push"},
    {"timer-set", 0x406490, "game/TitleMenu.hpp（Timer=0x78 直接赋值）"},
    {"anm-find-child", 0x46F510, "game/AnmManager.cpp AnmManager::find_child"},
    {"anm-interrupt", 0x46F0B0, "game/AnmManager.cpp AnmManager::interrupt"},
    {"anm-interrupt-run", 0x46F130, "game/AnmManager.cpp AnmManager::interrupt_now"},
    {"anm-kill-file", 0x46F270, "game/AnmManager.cpp AnmManager::kill_file"},
    {"anm-unload-file", 0x46D770, "game/AnmManager.cpp AnmManager::unload"},
    // —— 已定位，待还原 ——
    {"ecl-dispatch", 0x41DCB0, nullptr},     // 19976 字节、81 个被调、switch 密集：ECL 指令主分发
    {"effect-attach", 0x418AF0, nullptr},    // ins_508：EffectManager（DAT_004a6db8）按效果种类表 DAT_004a2250 挂 update/draw 回调（effect.anm 155–196 依赖）
    {"effect-ring-update", 0x405700, nullptr}, // 效果种类 1 的 update 回调（DAT_004919e8[2]）：环形顶点缓冲 + 生成 effect.anm 0x99
    {"anm-load-file", 0x46D020, nullptr},    // AnmManager::load_file(slot, name)：从档案读 .anm 并进 bank（同步等待装载线程）
    {"anm-draw-mode4+", 0x466390, nullptr},  // mode 4+（投影 / 世界 / 条带 / 网格）：0x466390 / 0x466820 / 0x4671b0 / …
    {"title-has-clear", 0x44A850, nullptr},  // 任一难度通关记录 → Extra 可选（当前恒 0：无存档）
    {"title-demo", 0x44AF80, nullptr},       // 0x44af80 开头：无输入 0x708 帧后播放 demo 录像（DAT_004a5bf0）
    {"title-option", 0x44C570, nullptr},     // 画面 3：Option
    {"title-difficulty", 0x44FE20, nullptr}, // 画面 5：难度选择（0/1/2 项入口）
    {"title-replay", 0x451750, nullptr},     // 画面 0xb
    {"title-score", 0x452330, nullptr},      // 画面 0xa：Player Data
    {"title-music-room", 0x4546F0, nullptr}, // 画面 0xd
    {"title-manual", 0x4545A0, nullptr},     // 画面 0x10
    {"title-spell-practice", 0x4553D0, nullptr}, // 画面 0x11
    {"score-file", 0x4497E0, nullptr},       // scoreth16.dat 读写
    {"replay-file", 0x4403C5, nullptr},      // th16_%.2d.rpy
    {"ver-file", 0x43B480, nullptr},         // th16_%.4x%c.ver
    {"music-fmt", 0x43AEAE, nullptr},        // ../../bgm/thbgm.fmt
    // —— 未定位 ——
    {"enemy-manager", 0, nullptr},
    {"bullet-manager", 0, nullptr},
    {"item-manager", 0, nullptr},
    {"player", 0, nullptr},
    {"season-system", 0, nullptr},  // th16 特有：季节槽/季节解放
};

}  // namespace th16::rebuild
