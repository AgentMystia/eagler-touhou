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
    {"boot-scene-init", 0x43ADC0, "sdl/Application.cpp Application::enter_boot（sig.anm → 槽 1）"},
    {"boot-scene-draw", 0x43B300, "sdl/Application.cpp Application::step（boot_frames++ / spawn）"},
    {"boot-scene-update", 0x43B290, "game/Supervisor.hpp Supervisor::boot_init_finished（请求 Title=4）"},
    {"supervisor-register", 0x43BA40, "game/Supervisor.hpp（主更新 0x43b3d0 / 场景切换 0x43ce10）"},
    {"supervisor-frame", 0x43B3D0, "sdl/Application.cpp Application::step"},
    {"supervisor-switch", 0x43CE10, "game/Supervisor.hpp Supervisor::consume_switch"},
    {"update-chain-walk", 0x401460, nullptr},  // DAT_004a6d94 更新链遍历（待接入完整 Chain 表）
    {"draw-chain-walk", 0x4015A0, nullptr},    // 绘制链遍历
    {"input-sample", 0x401D50, "game/Input.hpp InputState（键盘位收集）"},
    {"input-edges", 0x405580, "game/Input.hpp InputState::commit"},
    {"title-scene-create", 0x44AEE0, "sdl/Application.cpp Application::enter_title_assets"},
    {"title-scene-thread", 0x44ABC0, "sdl/Application.cpp Application::activate_title_menu（等 0xb4 帧）"},
    {"title-scene-init", 0x44AC70, "sdl/Application.cpp Application::enter_title_assets（title.anm / title_v.anm）"},
    {"title-menu-anm", 0x44B5F0, "sdl/Application.cpp Application::activate_title_menu（脚本 0x61/0x65/0 子集）"},
    {"anm-kill-file", 0x46F270, "game/AnmManager.cpp AnmManager::kill_file"},
    {"anm-unload-file", 0x46D770, "game/AnmManager.cpp AnmManager::unload"},
    // —— 已定位，待还原 ——
    {"ecl-dispatch", 0x41DCB0, nullptr},     // 19976 字节、81 个被调、switch 密集：ECL 指令主分发
    {"effect-attach", 0x418AF0, nullptr},    // ins_508：EffectManager（DAT_004a6db8）按效果种类表 DAT_004a2250 挂 update/draw 回调（effect.anm 155–196 依赖）
    {"effect-ring-update", 0x405700, nullptr}, // 效果种类 1 的 update 回调（DAT_004919e8[2]）：环形顶点缓冲 + 生成 effect.anm 0x99
    {"anm-load-file", 0x46D020, nullptr},    // AnmManager::load_file(slot, name)：从档案读 .anm 并进 bank（同步等待装载线程）
    {"anm-draw-mode4+", 0x466390, nullptr},  // mode 4+（投影 / 世界 / 条带 / 网格）：0x466390 / 0x466820 / 0x4671b0 / …
    {"title-menu-full", 0x44AF80, nullptr},  // 标题菜单完整状态机（子菜单 / demo / 音乐室…）
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
