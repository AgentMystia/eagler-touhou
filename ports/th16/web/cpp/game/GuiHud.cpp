// th16 Gui——HUD 部分（原版 DAT_004a6dcc 的 VM 字段；对话部分见 Gui.cpp）。
//
//   0x426d70  关卡首帧建 HUD VM、刷新残机 / 炸弹 / 季节量表显示          → Gui::init_hud
//   0x42c920  残机 / 炸弹格：UI 链表 VM + 指针出参                       → AnmManager::spawn_ui(out_vm)
//   0x42c280  残机显示刷新                                               → Gui::refresh_lives_display
//   0x42c390  炸弹显示刷新                                               → Gui::refresh_bombs_display
//   0x42c600  季节量表显示刷新（0x43df20 进度）                          → Gui::refresh_season_gauge
//   0x42bcf0  弹出通知（符卡奖励 / 取得失败 / 满火力 / extend）          → Gui::show_notice
//   0x427cf0  每帧 HUD 段：文字底存活、面板让位、Boss 血条、符卡星、Boss 方位标、炸弹可用标
//   0x42c8f0 / 0x42c8c0 / 0x42c890 / 0x418440 / 0x418470  计时数字移位 → Gui::move_timer_digit
//   0x42c580 / 0x42c5c0  HUD 根 VM 隐藏 / 显示（+0x12c 在 th16 无创建路径）→ Gui::hide_hud / show_hud
//
// 原版 Gui+0x1ac bit11 / bit12（符卡奖励滚动显示，0x42c4f0 收尾）在 th16 没有任何置位方，属前作遗留的死代码，不移植。
#include "Gui.hpp"

#include "AnmManager.hpp"
#include "Bomb.hpp"
#include "Enemy.hpp"
#include "GameGlobals.hpp"
#include "Motion.hpp"
#include "Player.hpp"
#include "Spellcard.hpp"

namespace th16 {

namespace {

constexpr float kTwoPi = 6.2831855f;
constexpr float kPi = 3.1415927f;
constexpr float kBossBarMarkerRadius = 112.0f;
constexpr float kBossBarApproachStep = 0.025f;
// 自机与 Boss 的距离平方：小于 80² 时血条整套淡出，大于等于 96² 时恢复（滞回）
constexpr float kBossBarFadeDistanceSq = 6400.0f;
constexpr float kBossBarRestoreDistanceSq = 9216.0f;

}  // namespace

u32 Gui::spawn_world(AnmFile& file, i32 script) {
    return animations->spawn(file, script, -1);
}

u32 Gui::spawn_hidden_world(AnmFile& file, i32 script, AnmVm** out_vm) {
    // 0x426d70 计时数字：世界链表 VM、creation_flags 0；建好后整树隐藏并清 flags_hi 的 0xc0000
    const u32 id = animations->spawn(file, script, -1, out_vm);
    AnmVm& vm = **out_vm;
    AnmManager::hide_tree(vm);
    vm.flags_hi &= ~0xc0000u;
    return id;
}

AnmVm* Gui::panel_child(i32 script) {
    AnmVm* panel = find_or_clear(panel_vm);
    return panel != nullptr ? AnmManager::find_child(*panel, script, 0) : nullptr;
}

// 0x426d70
void Gui::init_hud(AnmFile* ascii) {
    if (animations == nullptr || front == nullptr || globals == nullptr) return;
    if (hud_frame_vm == 0) hud_frame_vm = animations->spawn_ui(*front, kHudFrame);
    if (life_vms[0] == nullptr) {
        for (int i = 0; i < kResourceSlots; ++i) life_vm_ids[i] = animations->spawn_ui(*front, kLifeSlot0 + i, &life_vms[i]);
        for (int i = 0; i < kResourceSlots; ++i) bomb_vm_ids[i] = animations->spawn_ui(*front, kBombSlot0 + i, &bomb_vms[i]);
        if (ascii != nullptr) {
            for (int i = 0; i < 2; ++i) boss_timer_vm_ids[i] = spawn_hidden_world(*ascii, 2 + i, &boss_timer_vms[i]);
        }
    }
    refresh_lives_display(globals->lives, globals->life_fragments);
    refresh_bombs_display(globals->bombs, globals->bomb_fragments);

    // 关卡标题（logo.anm 脚本 1）：符卡练习与演示回放不出；id 不保存，由脚本自行结束
    const bool demo = (globals->flags & 0x40u) != 0;
    if (globals->scene_request != 8 && !demo && (globals->flags & GameGlobals::kModeMask) != 0x20 && logo != nullptr) {
        spawn_world(*logo, 1);
    }
    if (demo) spawn_world(*front, kDemoBanner);
    if (boss_indicator_vm == 0) boss_indicator_vm = spawn_world(*front, kBossIndicator);
    // 第 1 面、非回放、未续关：操作提示；魔理沙的提示位置更低
    if (globals->stage == 1 && !replay_playback && globals->continues == 0) {
        AnmVm* hint = nullptr;
        animations->spawn(*front, kStageOneHint, -1, &hint);
        hint->entity_offset = Vec3{0.0f, globals->shot == 3 ? 148.0f : 128.0f, 0.0f};
    }
    if (globals->new_game) {
        difficulty_label_vm = spawn_world(*front, kDifficultyLabel0 + globals->difficulty);
        animations->interrupt(difficulty_label_vm, 3);
    }
    difficulty_badge_vm = spawn_world(*front, kDifficultyBadge0 + globals->difficulty);
    // 名牌与其子 VM 再挂一次中断 3（原版此处重复，保留）
    if (AnmVm* label = animations->find(difficulty_label_vm)) {
        animations->request_interrupt(*label, 3);
        for (AnmListNode* n = label->children.next; n != nullptr; n = n->next) {
            if (n->owner != nullptr) animations->request_interrupt(*n->owner, 3);
        }
    }
    spell_star_count = 0;
    boss_bars[0].created = 0;
    boss_bars[1].created = 0;
    if (globals->new_game) {
        bomb_ready_shown = 0;
        panel_vm = spawn_world(*front, kHudPanel);
        if (AnmVm* icon = panel_child(kSeasonIcon)) {
            if (AnmFile* file = animations->file(icon->file_slot)) anm_set_sprite(*file, *icon, globals->season + 0x52);
        }
    }
    refresh_season_gauge();
    if (vm_110 != 0) {
        animations->interrupt(vm_110, 1);
        vm_110 = 0;
    }
}

// 0x42c280
void Gui::refresh_lives_display(i32 lives, i32 fragments) {
    if (life_vms[0] == nullptr || animations == nullptr || globals == nullptr) return;
    i32 lit = 0;
    if (lives > 0) {
        for (; lit < lives; ++lit) animations->request_interrupt(*life_vms[lit], 2);
        if (lives > 7) return;
    }
    // 第 lives 格按碎片数分段点亮：中断 7 + 碎片×5 / 本级 extend 所需碎片
    const u32 needed = u32(globals->extend_threshold());
    const i32 fragment_level = i32(i16(u32(fragments * 5) / needed));
    animations->request_interrupt(*life_vms[lit], fragment_level + 7);
    for (++lit; lit < kResourceSlots; ++lit) animations->request_interrupt(*life_vms[lit], 3);
}

// 0x42c390
void Gui::refresh_bombs_display(i32 bombs, i32 fragments) {
    if (bomb_vms[0] == nullptr || animations == nullptr) return;
    i32 lit = 0;
    if (bombs > 0) {
        for (; lit < bombs; ++lit) animations->request_interrupt(*bomb_vms[lit], 2);
        if (bombs > 7) return;
    }
    animations->request_interrupt(*bomb_vms[lit], i32(i16(fragments)) + 7);
    for (++lit; lit < kResourceSlots; ++lit) animations->request_interrupt(*bomb_vms[lit], 3);
}

// 0x42c600
void Gui::refresh_season_gauge() {
    if (animations == nullptr || globals == nullptr) return;
    AnmVm* bar = panel_child(kGaugeBar);
    AnmVm* level_vm = panel_child(kGaugeLevel);
    if (bar == nullptr || level_vm == nullptr) return;   // 原版不判空：面板只在新开局建，此后一直存在
    const i32 level = globals->season_option_count();
    if (level == 0) {
        bar->sprite_size.x = globals->season_gauge_fraction() * 100.0f;
        bar->flags_lo |= 8u;
        AnmManager::hide_tree(*level_vm);
        if (gauge_level_shown == 1) animations->interrupt_now(*bar, 3);
        gauge_level_shown = 0;
        return;
    }
    bar->sprite_size.x = globals->season_gauge < globals->season_gauge_max ? globals->season_gauge_fraction() * 100.0f : 100.0f;
    bar->flags_lo |= 8u;
    AnmManager::show_tree(*level_vm);
    animations->request_interrupt(*level_vm, level + 7);
    if (gauge_level_shown == 0) animations->interrupt_now(*bar, 2);
    gauge_level_shown = 1;
}

// 0x42bcf0
void Gui::show_notice(Notice kind, i32 value) {
    if (animations == nullptr || front == nullptr) return;
    AnmFile* ascii = animations->file(AnmManager::kAsciiSlot);
    switch (kind) {
    case Notice::SpellBonus: {
        kill_vm(notice_vm);
        notice_vm = spawn_world(*front, kNoticeSpellBonus);
        // 8 位数字（ascii.anm 4..0xb，精灵 0xef + 数字），前导零整树隐藏
        i32 remaining = value;
        i32 divisor = 10000000;
        bool significant = false;
        for (int i = 0; i < kNoticeDigits; ++i) {
            kill_vm(notice_digit_vms[i]);
            if (ascii == nullptr) continue;
            notice_digit_vms[i] = spawn_world(*ascii, 4 + i);
            const i32 digit = remaining / divisor;
            remaining %= divisor;
            divisor /= 10;
            if (digit != 0) significant = true;
            if (AnmVm* vm = animations->find(notice_digit_vms[i])) {
                if (AnmFile* file = animations->file(vm->file_slot)) anm_set_sprite(*file, *vm, digit + 0xef);
                if (significant) AnmManager::show_tree(*vm);
                else AnmManager::hide_tree(*vm);
            }
        }
        // 百万位 / 千位逗号（精灵 0xfd）
        for (int i = 0; i < 2; ++i) {
            kill_vm(notice_comma_vms[i]);
            if (ascii == nullptr || value < (i == 0 ? 1000000 : 1000)) continue;
            notice_comma_vms[i] = spawn_world(*ascii, 0xc + i);
            if (AnmVm* vm = animations->find(notice_comma_vms[i])) {
                if (AnmFile* file = animations->file(vm->file_slot)) anm_set_sprite(*file, *vm, 0xfd);
            }
        }
        notice_label_active = 1;
        notice_label_vm = spawn_world(*front, kNoticeLabel);
        return;
    }
    case Notice::SpellFailed:
        kill_vm(notice_vm);
        notice_vm = spawn_world(*front, kNoticeSpellFailed);
        notice_label_active = 1;
        notice_label_vm = spawn_world(*front, kNoticeLabel);
        return;
    case Notice::FullPower:
    case Notice::Kind3:
    case Notice::Extend:
        kill_vm(notice_small_vm);
        notice_small_vm = spawn_world(*front, kNoticeFullPower + (i32(kind) - i32(Notice::FullPower)));
        return;
    case Notice::Kind6:
        kill_vm(notice_vm);
        notice_vm = spawn_world(*front, kNoticeKind6);
        return;
    case Notice::None:
        return;
    }
}

void Gui::celebrate_extend() {
    if (globals != nullptr) refresh_lives_display(globals->lives, globals->life_fragments);
    play_sfx(0x11);
    show_notice(Notice::Extend, 0);
}

void Gui::dismiss_notice() {
    if (animations != nullptr) animations->interrupt(notice_vm, 1);
}

void Gui::set_boss_bar_marker(i32 slot, i32 index, float ratio, i32 flag) {
    if (slot < 0 || slot >= kBossBarSlots || index < 0 || index >= kBossBarMarkers) return;
    boss_bars[slot].markers[index] = ratio;
    boss_bars[slot].marker_flags[index] = flag;
}

void Gui::set_boss_timer_style(TimerStyle style) {
    if (animations == nullptr) return;
    for (AnmVm* vm : boss_timer_vms) {
        if (vm != nullptr) animations->interrupt_now(*vm, i32(style));
    }
}

// 0x42c8f0（5 让位）/ 0x42c8c0（4 归位）只挂中断；0x418440 / 0x418470 + 0x42c890 立即执行（符卡中 2、否则 3，再 4）
void Gui::move_timer_digit(AnmVm* vm, TimerDigitMove move) {
    if (vm == nullptr || animations == nullptr) return;
    switch (move) {
    case TimerDigitMove::Aside: animations->request_interrupt(*vm, 5); return;
    case TimerDigitMove::Back: animations->request_interrupt(*vm, 4); return;
    case TimerDigitMove::Reset: {
        const bool spell_active = spellcard_flags != nullptr && (*spellcard_flags & Spellcard::kActive) != 0;
        animations->interrupt_now(*vm, spell_active ? 2 : 3);
        animations->interrupt_now(*vm, 4);
        return;
    }
    }
}

// 0x42c580 / 0x42c5c0：目标是 +0x12c 的 HUD 根 VM，th16 从不创建它
void Gui::hide_hud() {}
void Gui::show_hud() {}

// 0x427cf0 段 (c)
void Gui::update_notice_label() {
    if (notice_label_active == 0) return;
    if (animations->find(notice_label_vm) == nullptr) {
        notice_label_vm = 0;
        notice_label_active = 0;
    }
}

// 0x427cf0 段 (d)：自机进到面板下方区域（y > 400 且 x < -64）时面板中断 5 让位，离开后中断 4 归位
void Gui::slide_hud_for_player() {
    if (player == nullptr) return;
    const bool near_panel = player->pos.y > 400.0f && player->pos.x < -64.0f;
    if ((flags_1ac & 1u) == 0) {
        if (near_panel) {
            animations->interrupt(panel_vm, 5);
            flags_1ac |= 1u;
        }
    } else if (player->pos.y < 384.0f || player->pos.x > -64.0f) {
        animations->interrupt(panel_vm, 4);
        flags_1ac &= ~1u;
    }
}

void Gui::retire_boss_bar(BossBar& bar) {
    if (bar.created == 0) return;
    for (u32& id : bar.vm_ids) {
        animations->kill(id);
        id = 0;
    }
    bar.created = 0;
}

// 0x427cf0 段 (f)
void Gui::update_boss_bars() {
    if (enemies == nullptr || enemies->hud_hidden) return;
    for (int slot = 0; slot < kBossBarSlots; ++slot) {
        BossBar& bar = boss_bars[slot];
        Enemy* boss = enemies->boss_ids[slot] != 0 ? enemies->find_id(enemies->boss_ids[slot]) : nullptr;
        if (boss == nullptr) {
            bar.shown = 0.0f;
            for (float& marker : bar.markers) marker = 0.0f;
            retire_boss_bar(bar);
            if (slot == 0) kill_vm(boss_name_vm);
            continue;
        }
        const bool visible = boss->life < 100000 && (boss->flags & 0x31u) == 0 && boss->invuln.current < 1 && dialogue == nullptr;
        if (!visible) {
            retire_boss_bar(bar);
            continue;
        }
        bar.life = boss->life;
        const float target = float(boss->life) / float(boss->life_max);
        bar.target = target;
        if (bar.shown < target) bar.shown = bar.shown + kBossBarApproachStep;
        if (target < bar.shown) bar.shown = target;
        if (bar.created == 0) {
            static constexpr i32 kScripts[kBossBarVms] = {kBossRing, kBossRingBase, kBossRingFace,
                                                          kBossRingMarker, kBossRingMarker, kBossRingMarker, kBossRingMarker};
            for (int i = 0; i < kBossBarVms; ++i) bar.vm_ids[i] = spawn_world(*front, kScripts[i]);
            bar.created = 1;
        }
        ensure_boss_name();
        // 环 / 底 / 面跟随 Boss（场地坐标 ×2 = 渲染目标像素），环按显示比例转动
        const Vec3 center{boss->pos.x * 2.0f, boss->pos.y * 2.0f, boss->pos.z};
        if (AnmVm* ring = find_or_clear(bar.vm_ids[0])) {
            ring->flags_lo |= 4u;
            ring->rotation.x = bar.shown * -kTwoPi;
            ring->entity_offset = center;
        }
        for (int i = 1; i < 3; ++i) {
            if (AnmVm* vm = find_or_clear(bar.vm_ids[i])) vm->entity_offset = center;
        }
        // 刻度：比例为 0 或已被显示比例越过的隐藏；其余放到环上对应角度（-π 起顺时针）
        float marker_z = 0.0f;
        for (int i = 0; i < kBossBarMarkers; ++i) {
            AnmVm* marker = find_or_clear(bar.vm_ids[3 + i]);
            if (marker == nullptr) continue;
            const float ratio = bar.markers[i];
            if (ratio == 0.0f || bar.shown <= ratio) {
                AnmManager::hide_tree(*marker);
                continue;
            }
            AnmManager::show_tree(*marker);
            const float angle = wrap_angle(-kPi - ratio * kTwoPi);
            marker->flags_lo |= 4u;
            marker->rotation.z = angle;
            const float s = sin_f(angle);
            const float c = cos_f(angle);
            marker_z = boss->pos.z + marker_z;
            marker->entity_offset = Vec3{(s * 0.0f - c * kBossBarMarkerRadius) + center.x,
                                         (s * kBossBarMarkerRadius + c * 0.0f) + center.y, marker_z};
        }
        // 自机贴近 Boss：整套中断 3（淡出）；离开到 96 以外：中断 2（恢复）
        if (player != nullptr) {
            const Enemy* slot_boss = enemies->boss_slot(slot);
            const float dx = slot_boss->pos.x - player->pos.x;
            const float dy = slot_boss->pos.y - player->pos.y;
            const float dist_sq = dx * dx + dy * dy;
            if (bar.near_player == 0) {
                if (dist_sq < kBossBarFadeDistanceSq) {
                    for (u32 id : bar.vm_ids) animations->interrupt(id, 3);
                    bar.near_player = 1;
                }
            } else if (dist_sq >= kBossBarRestoreDistanceSq) {
                for (u32 id : bar.vm_ids) animations->interrupt(id, 2);
                bar.near_player = 0;
            }
        }
    }
}

// 0x427cf0 段 (g)
void Gui::update_spell_stars() {
    for (int i = 0; i < kSpellStarSlots; ++i) {
        u32& id = spell_star_vms[i];
        if (i < spell_star_count) {
            if (id == 0) id = spawn_world(*front, kSpellStar0 + i);
        } else if (id != 0) {
            animations->interrupt(id, 1);
            id = 0;
        }
    }
}

// 0x427cf0 段 (i)：Boss 槽 0 存活且未隐藏（flags bit5）、未标记 bit0 时显示方位标
void Gui::update_boss_indicator() {
    if (enemies == nullptr) return;
    Enemy* boss = enemies->boss_ids[0] != 0 ? enemies->find_id(enemies->boss_ids[0]) : nullptr;
    if (boss == nullptr || (boss->flags & 0x20u) != 0 || (boss->flags & 0x1u) != 0) {
        if (AnmVm* vm = animations->find(boss_indicator_vm)) AnmManager::hide_tree(*vm);
        return;
    }
    AnmVm* vm = find_or_clear(boss_indicator_vm);
    if (vm == nullptr) return;   // 原版不判空（方位标 VM 由 init_hud 保证存在）
    AnmManager::show_tree(*vm);
    // 血量阶段：非符卡 700 / 400 / 200 / 200，符卡 2000 / 1000 / 400 / 400；flags_1ac bit1-2 记录已到的阶段
    const bool spell_active = spellcard_flags != nullptr && (*spellcard_flags & Spellcard::kActive) != 0;
    const i32 life = boss->life_display;
    const u32 phase = flags_1ac & 6u;
    if (!spell_active) {
        if (phase == 0) {
            if (life < 700) { animations->request_interrupt(*vm, 7); flags_1ac = (flags_1ac & ~4u) | 2u; }
        } else if (phase == 2) {
            if (life < 400) { animations->request_interrupt(*vm, 8); flags_1ac = (flags_1ac & ~2u) | 4u; }
        } else if (phase == 4) {
            if (life < 200) { animations->request_interrupt(*vm, 9); flags_1ac |= 6u; }
        } else if (phase == 6) {
            if (life > 200) { animations->request_interrupt(*vm, 10); flags_1ac &= ~6u; }
        }
    } else {
        if (phase == 0) {
            if (life < 2000) { animations->request_interrupt(*vm, 7); flags_1ac = (flags_1ac & ~4u) | 2u; }
        } else if (phase == 2) {
            if (life < 1000) { animations->request_interrupt(*vm, 8); flags_1ac = (flags_1ac & ~2u) | 4u; }
        } else if (phase == 4) {
            if (life < 400) { animations->request_interrupt(*vm, 9); flags_1ac |= 6u; }
        } else if (phase == 6) {
            if (life > 400) { animations->request_interrupt(*vm, 10); flags_1ac &= ~6u; }
        }
    }
    // 位置：场地 x 换算到渲染目标像素（(x + 224) × 2），y 固定 960；与自机横向距离 < 64 时按距离变淡；
    // Boss 出了场地横向范围则完全透明
    vm->entity_offset.y = 960.0f;
    const float screen_x = boss->pos.x + 32.0f + 192.0f;
    vm->entity_offset.x = screen_x + screen_x;
    float dx = 0.0f;
    if (player != nullptr) dx = boss->pos.x - player->pos.x;
    if (dx < 0.0f) dx = -dx;
    vm->color[3] = dx < 64.0f ? u8(i32(dx * 191.0f * 0.015625f) + 0x40) : u8(0xff);
    if (boss->pos.x < -192.0f || boss->pos.x > 192.0f) vm->color[3] = 0;
}

// 0x427cf0 段 (j)
void Gui::update_bomb_ready_mark() {
    if (season_release != nullptr && season_release->usable()) {
        if (bomb_ready_shown == 0) {
            AnmVm* mark = panel_child(kBombReadyMark);
            animations->interrupt_now(mark != nullptr ? mark->id : 0u, 2);
        }
        bomb_ready_shown = 1;
        return;
    }
    if (bomb_ready_shown == 1) {
        AnmVm* mark = panel_child(kBombReadyMark);
        animations->interrupt_now(mark != nullptr ? mark->id : 0u, 3);
    }
    bomb_ready_shown = 0;
}

}  // namespace th16
