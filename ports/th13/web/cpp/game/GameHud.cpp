// GameHud 生命周期：建句柄、interrupt 布局（与 th15 GameHud.cpp 逐行同构；
// 脚本号按 th13 front.anm dump 锁定：残机 script30–37、灵击 script38–45）。
#include "GameHud.hpp"

namespace th13 {

bool GameHud::check(bool ok) {
    if (!ok && error.empty())
        error = animations.error.empty() ? "HUD animation unavailable" : animations.error;
    return ok;
}

u32 GameHud::create(i32 bank, i32 script, u32 ordering) {
    const u32 handle = animations.create(bank, script, -1, ordering);
    check(handle != 0);
    return handle;
}

u32 GameHud::widget(i32 script) {
    const u32 handle = create(front, script, 0);
    if (auto* vm = animations.registry.find(handle)) vm->visual.render_flags &= ~0xc000u;
    return handle;
}

bool GameHud::direct_interrupt(u32 handle, i32 label) {
    auto* vm = animations.registry.find(handle);
    if (!vm) return check(false);
    vm->pending_interrupt = label;
    return true;
}

bool GameHud::life(i32 lives, i32 pieces) {
    if (!life_icons[0]) return true;
    if (lives > 8 || lives < 0) {
        error = "HUD life values outside original limits";
        return false;
    }
    for (i32 i = 0; i < lives; i++)
        if (!direct_interrupt(life_icons[u32(i)], 2)) return false;
    if (lives > 7) return true;
    // 碎片阈值 5：front.anm script30 标签 7..12 ⇒ 碎片 0..5（dump anm-list.txt:6643）。
    if (!direct_interrupt(life_icons[u32(lives)], pieces + 7)) return false;
    for (i32 i = lives + 1; i < 8; i++)
        if (!direct_interrupt(life_icons[u32(i)], 3)) return false;
    return true;
}

bool GameHud::bombs(i32 stock, i32 pieces) {
    if (!bomb_icons[0]) return true;
    if (stock < 0 || stock > 8) {
        error = "HUD bomb stock outside original limits";
        return false;
    }
    for (i32 i = 0; i < stock; i++)
        if (!direct_interrupt(bomb_icons[u32(i)], 2)) return false;
    if (stock > 7) return true;
    if (!direct_interrupt(bomb_icons[u32(stock)], pieces + 7)) return false;
    for (i32 i = stock + 1; i < 8; i++)
        if (!direct_interrupt(bomb_icons[u32(i)], 3)) return false;
    return true;
}

bool GameHud::stage_logo(i32 scene_destination) {
    // Stage 场景资产（st01logo 等）随 Stage 分期接入；UI 基座仅认非转场入口。
    return scene_destination == 8;
}

bool GameHud::initialize(const HudEntryContext& context) {
    if (!error.empty()) return false;
    if (!root && (root = widget(0)) == 0) return false;
    if (!life_icons[0]) {
        for (u32 i = 0; i < 8; i++) {
            life_icons[i] = create(front, i32(i) + 30, 0);
            auto* vm = animations.registry.find(life_icons[i]);
            if (!vm) return false;
            vm->visual.render_flags &= ~0xc000u;
        }
        for (u32 i = 0; i < 8; i++) {
            bomb_icons[i] = create(front, i32(i) + 38, 0);
            auto* vm = animations.registry.find(bomb_icons[i]);
            if (!vm) return false;
            vm->visual.render_flags &= ~0xc000u;
        }
    }
    if (!life(player.extra_lives, player.life_pieces) || !bombs(player.bombs, player.bomb_pieces))
        return false;
    // 难度标签/通知句柄：脚本号待对 dump 定位后接入（th15 为 front+80/+86），
    // 接口先按 th15 形状保留（ui-base.md §五）。
    (void)context;
    return true;
}

bool GameHud::reset_for_retry() {
    prepare_stage_assets();
    return life(player.extra_lives, player.life_pieces) && bombs(player.bombs, player.bomb_pieces);
}

}  // namespace th13
