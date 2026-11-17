#include "Player.hpp"

#include "AnmManager.hpp"
#include "GameGlobals.hpp"
#include "Input.hpp"

namespace th16 {

namespace {

// DAT_00492c20：方向 → (dx, dy)
constexpr i32 kDirectionTable[9][2] = {{0, 0}, {0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
// DAT_00492c00 / DAT_00492c10：子机脚本 / 满火力附加脚本（按机体）；DAT_00492be0：子季节子机脚本（按季节）
constexpr int kOptionScript[4] = {8, 7, 11, 11};
constexpr int kOptionFrontScript[4] = {9, 8, 12, 13};
constexpr int kSubOptionScript[5] = {1, 1, 1, 18, 1};
// DAT_004a5e4c（机体 × 等级 → 子机位置起始下标）/ DAT_004a5dac（季节 × 子机数）
constexpr i32 kOptionSlotBase[4][8] = {
    {28, 0, 1, 3, 6, 10, 11, 13}, {16, 0, 1, 3, 6, 10, 11, 13}, {16, 0, 1, 3, 6, 10, 11, 13}, {16, 0, 1, 3, 6, 10, 11, 13}};
constexpr i32 kSubOptionSlotBase[5][8] = {
    {0, 0, 1, 3, 6, 10, 15, 21}, {28, 0, 1, 3, 6, 10, 15, 21}, {28, 0, 1, 3, 6, 10, 15, 21},
    {28, 0, 1, 3, 6, 10, 15, 21}, {28, 0, 1, 3, 6, 10, 15, 21}};
// DAT_00492c98 / DAT_00492c78 / DAT_00492c88 / DAT_00492c68：判定 / 擦弹 / 收点 / 擦弹框半径（四机体同值）
constexpr float kHitboxRadius = 3.0f, kGrazeRadius = 5.0f, kItemRadius = 60.0f, kGrazeBoxRadius = 100.0f;
constexpr i32 kSeasonSteps[8] = {0, 100, 0x82, 0xa0, 200, 0xfa, 300, 0};
constexpr float kFixedToFloat = 0.0078125f;
constexpr int kFocusEffectScript = 0x1a, kSecondEffectScript = 0x1b;

inline Vec3 sub3(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 add3(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

}  // namespace

void Player::set_position(float x, float y) {
    // 0x440e40
    fixed_x = i32(x * 128.0f);
    fixed_y = i32(y * 128.0f);
    pos.x = float(fixed_x) * kFixedToFloat;
    pos.y = float(fixed_y) * kFixedToFloat;
    for (PlayerOption& o : options) o.snap = 1;
}

void Player::recompute_rects_init() {
    // 0x440fb0 尾部：第二组矩形用整幅半径，且 +0x2c80c 组用擦弹框半径
    rect_min = sub3(pos, hit_radius);
    rect_max = add3(pos, hit_radius);
    item_rect_min = sub3(pos, item_radius);
    item_rect_max = add3(pos, item_radius);
    graze_rect_min = sub3(pos, graze_radius);
    graze_rect_max = add3(pos, graze_radius);
    item_rect2_min = sub3(pos, graze_radius);
    item_rect2_max = add3(pos, graze_radius);
}

void Player::reset_shot_timers() {
    // 0x165cc / 0x165e0：previous = -2, current = -1, current_f = -1.0
    for (Timer* t : {&shot_timer, &shot_timer2}) {
        if ((t->flags & 1) == 0) { t->current = 0; t->previous = i32(0xfff0bdc1); t->current_f = 0; t->speed_slot = 0; t->flags |= 1; }
        t->current = -1;
        t->current_f = -1.0f;
        t->previous = -2;
    }
}

bool Player::init(AnmManager& manager, GameGlobals& globals, AnmFile* player_anm, AnmFile* sub_player_anm,
                  const u8* sht_bytes, u32 sht_size, const u8* sub_sht_bytes, u32 sub_sht_size) {
    // 0x441c60：new + 0x4093f0 VM 构造 + 两实体 0x440ec0 + memset 0；0x440fb0 初始化
    anm = player_anm;
    sub_anm = sub_player_anm;
    if (anm == nullptr || sub_anm == nullptr) { error = "player anm missing"; return false; }
    if (!sht.parse(sht_bytes, sht_size)) { error = "pl sht: " + sht.error; return false; }
    if (!sub_sht.parse(sub_sht_bytes, sub_sht_size)) { error = "pl sub sht: " + sub_sht.error; return false; }

    // 0x407b20 脚本 0 → 自机 VM；回调清零；0x45f980 跑一帧
    manager.init_from_template(*anm, vm, 0);
    manager.run(vm);
    set_position(0.0f, 400.0f);
    speed = i32(sht.speed * 128.0f);
    speed_focus = i32(sht.speed_focus * 128.0f);
    speed_diag = i32(sht.speed_diag * 128.0f);
    speed_diag_focus = i32(sht.speed_diag_focus * 128.0f);
    sht.power_per_level = 100;
    globals.power_max = sht.power_per_level * sht.max_level;
    globals.power_per_level = sht.power_per_level;
    for (int i = 0; i < 8; ++i) globals.set_season_step(i, kSeasonSteps[i]);
    globals.season_gauge_max = globals.season_threshold[7];
    reset_shot_timers();
    sht.hitbox = kHitboxRadius;
    sht.item = kItemRadius;
    sht.graze = kGrazeRadius;
    hit_radius = {sht.hitbox * 0.5f, sht.hitbox * 0.5f, 5.0f};
    item_radius = {kItemRadius * 0.5f, kItemRadius * 0.5f, 5.0f};
    graze_radius = {kGrazeBoxRadius * 0.5f, kGrazeBoxRadius * 0.5f, 5.0f};
    recompute_rects_init();
    state_timer.reset();
    timer3.reset();
    invincible.reset();
    option_lag = kOptionLag;
    option_count = 0;
    speed_mult = 1.0f;
    for (PlayerOption& o : options) o.y = i32(0xffff3800);
    for (PlayerOption& o : sub_options) o.y = i32(0xffff3800);
    flags &= ~kFlagNoShot;
    scale_active = 0;
    scale = 1.0f;
    return true;
}

void Player::kill_vm(AnmManager& manager, u32& id) {
    manager.kill(id);
    id = 0;
}

void Player::interrupt_options(AnmManager& manager) {
    // 0x440dc0
    flags &= ~kFlagOptionsGathered;
    for (PlayerOption& o : options) { manager.interrupt(o.vm, 2); manager.interrupt(o.vm_front, 2); }
    for (PlayerOption& o : sub_options) { manager.interrupt(o.vm, 2); manager.interrupt(o.vm_front, 2); }
    gather_counter = 0;
}

void Player::reset(AnmManager& manager, GameGlobals& globals) {
    // 0x441740
    state = kNormal;
    reset_shot_timers();
    state_timer.reset();
    timer2.reset();
    timer3.reset();
    flags &= ~0x9u;
    kill_vm(manager, focus_vm);
    kill_vm(manager, effect_vm);
    // 0x42c280(DAT_004a6dcc, 残机, 残机碎片)：Gui 残机显示——Gui 接入时补
    interrupt_options(manager);
    setup_options(manager, globals);
    flags &= ~kFlagNoShot;
    speed_mult = 1.0f;
    last_damage_index = 0;
    scale_active = 0;
    scale = 1.0f;
    scale2 = 1.0f;
}

u32 Player::spawn_option_vm(AnmManager& manager, AnmFile& file, int script) {
    // 0x4440e0 内联：0x46f600 取池 → 0x407b20 → flags_hi = (hi & ~0x80000) | 0x40400 → entity_offset 0、layer 0xe、rot.z 0
    //            → 0x45f980 → creation_flags 0 → 0x46e7d0 世界链表尾；与 0x406380（layer < 0x18）路径逐位相同
    AnmVm* vm_ptr = nullptr;
    const u32 id = manager.spawn(file, script, 0xe, &vm_ptr);
    AnmVm* found = manager.find(id);
    if (found == nullptr) return 0;
    found->entity_offset.y = -32.0f;
    return id;
}

void Player::setup_options(AnmManager& manager, GameGlobals& globals) {
    // 0x4440e0（this = 实体 A）
    const int shot = globals.shot + globals.shot_aux;
    const i32 level = globals.power_per_level != 0 ? globals.power / globals.power_per_level : 0;
    power_level = level;
    if (globals.power < globals.power_max) {
        for (PlayerOption& o : options) { manager.interrupt(o.vm_front, 1); o.vm_front = 0; }
    } else if (level > 0) {
        for (int i = 0; i < level; ++i) {
            PlayerOption& o = options[i];
            manager.kill(o.vm_front);
            o.vm_front = 0;
            AnmVm* front = nullptr;
            const u32 id = manager.spawn_front(*anm, kOptionFrontScript[globals.shot], -1, &front);
            o.vm_front = manager.find(id) != nullptr ? id : 0;
            if (AnmVm* v = manager.find(id)) v->entity_offset.y = -32.0f;
        }
    }
    if (option_count != level) {
        int i = 0;
        for (; i < level && i < kOptionCount; ++i) {
            PlayerOption& o = options[i];
            o.x = fixed_x;
            o.y = fixed_y;
            manager.kill(o.vm);
            o.vm = 0;
            o.index = i;
            const i32 slot = kOptionSlotBase[shot][level] + i;
            o.offset_x = i32(sht.option_unfocused[slot].x * 128.0f);
            o.offset_y = i32(sht.option_unfocused[slot].y * 128.0f);
            o.focus_offset_x = i32(sht.option_focused[slot].x * 128.0f);
            o.focus_offset_y = i32(sht.option_focused[slot].y * 128.0f);
            const i32 ox = focus != 0 ? o.focus_offset_x : o.offset_x;
            const i32 oy = focus != 0 ? o.focus_offset_y : o.offset_y;
            o.target_x = o.x = fixed_x + ox;
            o.target_y = o.y = fixed_y + oy;
            o.vm = spawn_option_vm(manager, *anm, kOptionScript[globals.shot]);
            o.active = 2;
        }
        for (; i < kOptionCount; ++i) {
            PlayerOption& o = options[i];
            o.active = 0;
            manager.interrupt(o.vm, 1);
        }
        option_count = level;
        for (PlayerOption& o : options) o.snap = 1;
    }

    const i32 sub = globals.season_option_count();
    if (sub_count == sub) return;
    int i = 0;
    for (; i < sub && i < kSubOptionCount; ++i) {
        PlayerOption& o = sub_options[i];
        o.x = fixed_x;
        o.y = fixed_y;
        manager.kill(o.vm);
        o.vm = 0;
        o.index = i;
        const i32 slot = kSubOptionSlotBase[globals.season][sub] + i;
        o.offset_x = i32(sub_sht.option_unfocused[slot].x * 128.0f);
        o.offset_y = i32(sub_sht.option_unfocused[slot].y * 128.0f);
        o.focus_offset_x = i32(sub_sht.option_focused[slot].x * 128.0f);
        o.focus_offset_y = i32(sub_sht.option_focused[slot].y * 128.0f);
        const i32 ox = focus != 0 ? o.focus_offset_x : o.offset_x;
        const i32 oy = focus != 0 ? o.focus_offset_y : o.offset_y;
        o.target_x = o.x = fixed_x + ox;
        o.target_y = o.y = fixed_y + oy;
        o.vm = spawn_option_vm(manager, *sub_anm, kSubOptionScript[globals.season]);
        o.active = 2;
    }
    for (; i < kSubOptionCount; ++i) {
        PlayerOption& o = sub_options[i];
        o.active = 0;
        manager.interrupt(o.vm, 1);
    }
    sub_count = sub;
    for (PlayerOption& o : sub_options) o.snap = 1;
}

void Player::play_sprite(AnmManager& manager, int script) {
    // 0x407b20(anm, &vm, script) + 回调清零 + 0x45f980
    manager.init_from_template(*anm, vm, script);
    manager.run(vm);
}

void Player::update_options(AnmManager& manager, PlayerOption* list, int count) {
    // 0x442380
    for (int i = 0; i < count; ++i) {
        PlayerOption& o = list[i];
        if (o.active == 0) continue;
        if ((flags & kFlagOptionsGathered) == 0) {
            const bool foc = focus != 0;
            o.target_x = fixed_x + (foc ? o.focus_offset_x : o.offset_x);
            o.target_y = fixed_y + (foc ? o.focus_offset_y : o.offset_y);
            // +0xdc 移动回调（函数指针表）：th16 的 .sht 不使用，恒 0
        } else {
            o.target_x = fixed_x;
            o.target_y = fixed_y;
            if (gather_counter > 0x1d) {
                o.active = 0;
                manager.interrupt(o.vm, 1);
                manager.interrupt(o.vm_front, 1);
                continue;
            }
        }
        bool snap_now = false;
        if (o.snap == 0) {
            const i32 lag = option_lag;
            if (lag > 0x1d) {
                const i32 dx = ((o.target_x - o.x) * lag) / 100;
                const i32 dy = ((o.target_y - o.y) * lag) / 100;
                if (dx == 0 && dy == 0) snap_now = true;
                else { o.x += dx; o.y += dy; }
            }
        } else {
            o.snap = 0;
            snap_now = true;
        }
        if (snap_now) { o.x = o.target_x; o.y = o.target_y; }
        const float fx = float(o.x) * kFixedToFloat;
        const float fy = float(o.y) * kFixedToFloat;
        if (AnmVm* v = manager.find(o.vm)) v->entity_offset = {fx, fy, 0.0f};
        if (AnmVm* v = manager.find(o.vm_front)) v->entity_offset = {fx, fy, 0.0f};
    }
}

void Player::move(AnmManager& manager, const InputState& game_input, const PlayerWorld& world, AnmFile* effect_anm,
                  float game_speed) {
    // 0x441cf0
    const u32 held = game_input.held;
    const u8 lo = u8(held);
    if ((lo & 0x50) == 0x50) direction = 5;
    else if ((lo & 0x60) == 0x60) direction = 7;
    else if ((lo & 0x90) == 0x90) direction = 6;
    else if ((lo & 0xa0) == 0xa0) direction = 8;
    else if ((held & 0x20) != 0) direction = 2;
    else if ((held & 0x10) != 0) direction = 1;
    else if ((held & 0x40) != 0) direction = 3;
    else if ((lo & 0x80) != 0) direction = 4;
    else direction = 0;

    if (world.enemy_count == 0 || timer2.current < 4) {
        focus = 0;
        option_lag = kOptionLag;
    } else {
        focus = i32((held >> 3) & 1);
    }

    const i32 dir_x = kDirectionTable[direction][0];
    const i32 dir_y = kDirectionTable[direction][1];
    i32 spd;
    if (focus == 0) {
        if (manager.find(focus_vm) != nullptr) manager.interrupt(focus_vm, 1);
        focus_vm = 0;
        spd = direction < 5 ? speed : speed_diag;
    } else {
        if (focus_vm == 0 && effect_anm != nullptr) focus_vm = manager.spawn(*effect_anm, kFocusEffectScript, 0xe);
        AnmVm* fv = manager.find(focus_vm);
        if (fv == nullptr) {
            focus_vm = 0;
        } else {
            if ((flags & kFlagScaled) == 0) {
                fv->scale2 = {1.0f, 1.0f};
            } else {
                const float grown = (scale - 1.0f) + (scale - 1.0f);
                fv->scale2 = {grown + 1.0f, grown + 1.0f};
            }
            fv->flags_lo |= 8u;
        }
        spd = direction < 5 ? speed_focus : speed_diag_focus;
    }
    const i32 vx = i32(float(spd * dir_x - i32(push.x * -128.0f)) * speed_mult);
    const i32 vy = i32(float(spd * dir_y - i32(push.y * -128.0f)) * speed_mult);

    // 精灵切换：1 开始向左 / 3 开始向右 / 2 左→正 / 4 右→正
    if (vx < 0 && last_move_x >= 0) play_sprite(manager, 1);
    else if (vx > 0 && last_move_x <= 0) play_sprite(manager, 3);
    else if (vx == 0 && last_move_x < 0) play_sprite(manager, 2);
    else if (vx == 0 && last_move_x > 0) play_sprite(manager, 4);

    last_move_x = vx;
    last_move_y = vy;
    velocity.x = float(vx) * game_speed;
    velocity.y = float(vy) * game_speed;
    if (direction != 0) last_velocity = velocity;
    velocity_iy = i32(velocity.y);
    velocity_ix = i32(velocity.x);
    fixed_x += i32(velocity.x);
    fixed_y += i32(velocity.y);
    if (fixed_x < -0x5c00) fixed_x = -0x5c00;
    else if (fixed_x > 0x5c00) fixed_x = 0x5c00;
    if (fixed_y < 0x1000) fixed_y = 0x1000;
    else if (fixed_y > 0xd800) fixed_y = 0xd800;
    pos.x = float(fixed_x) * kFixedToFloat;
    pos.y = float(fixed_y) * kFixedToFloat;
    if (AnmVm* fv = manager.find(focus_vm)) fv->entity_offset = pos;
    else focus_vm = 0;

    if ((flags & kFlagOptionsGathered) != 0) ++gather_counter;
    update_options(manager, options, kOptionCount);
    update_options(manager, sub_options, kSubOptionCount);
    if (gather_counter > 0x1d) { option_count = 0; sub_count = 0; }

    if (effect_timer.current > 0) {
        if (manager.find(effect_vm) == nullptr) {
            effect_vm = 0;
            if (effect_anm != nullptr) effect_vm = manager.spawn(*effect_anm, kSecondEffectScript, 0xe);
        }
        if (AnmVm* ev = manager.find(effect_vm)) ev->entity_offset = pos;
        effect_timer.untick(game_speed);
        if (effect_timer.current < 1) {
            manager.interrupt(effect_vm, 1);
            effect_timer.reset();
            effect_vm = 0;
        }
    }
}

void Player::update(AnmManager& manager, GameGlobals& globals, const InputState& game_input, const PlayerWorld& world,
                    AnmFile* effect_anm, float game_speed) {
    // 0x442560
    (void)globals;
    switch (state) {
    case kEnter:
        // 入场：60 帧内从 y=480 升到 y=400；子机吸附
        fixed_y = 0xf000 - (state_timer.current * 0x2800) / 0x3c;
        pos.y = float(fixed_y) * kFixedToFloat;
        for (PlayerOption& o : options) o.snap = 1;
        // 0x431a70(pos_copy/pos, 0, 1|0)：道具吸引；0x416d20(pos, 0)：消弹——Item/Bullet 管理器接入时补
        if (state_timer.current > 0x3b) {
            state = kNormal;
            state_timer.set(0);
            move(manager, game_input, world, effect_anm, game_speed);
        }
        break;
    case kNormal:
        // 0x40dda0 / 0x40db20：炸弹可用判定与发动（输入位 2 / 0x800）——Bomb 管理器接入时补
        move(manager, game_input, world, effect_anm, game_speed);
        break;
    case kRespawnWait:
    case kState3:
    case kDying:
        // 被弹 → 复活流程依赖 Item / Effect / Gui 管理器，随之接入
        break;
    default:
        break;
    }

    // +0xd114 伤害源 ×257：无活动项（射击/敌弹接入后补）

    if (invincible.current < 1) {
        vm.flags_lo &= ~0x60000u;
        // flags 0x20（闪烁模式）与 speed_mult > 1.01 的残影特效（0x40e5c0）——Effect 管理器接入时补
    } else {
        invincible.untick(game_speed);
        if (state_timer.current == state_timer.previous || state_timer.current % 3 != 0) {
            vm.flags_lo &= ~0x60000u;
        } else {
            vm.color2[0] = 0xff; vm.color2[1] = 0x00; vm.color2[2] = 0x00; vm.color2[3] = 0xff;
            vm.flags_lo = (vm.flags_lo & ~0x40000u) | 0x20000u;
        }
    }
    speed_mult = 1.0f;
    push = {globals.push_x, globals.push_y, globals.push_z};
    manager.run(vm);

    if ((flags & kFlagScaled) == 0) {
        vm.flags_lo |= 8u;
        vm.scale = {1.0f, 1.0f};
        rect_min = sub3(pos, hit_radius);
        rect_max = add3(pos, hit_radius);
        item_rect_min = {pos.x - item_radius.x * 0.5f, pos.y - item_radius.y * 0.5f, pos.z - item_radius.z * 0.5f};
        item_rect_max = {pos.x + item_radius.x * 0.5f, pos.y + item_radius.y * 0.5f, pos.z + item_radius.z * 0.5f};
        graze_rect_min = sub3(pos, graze_radius);
        graze_rect_max = add3(pos, graze_radius);
        item_rect2_min = sub3(pos, item_radius);
        item_rect2_max = add3(pos, item_radius);
    } else {
        if (scale_active != 0) {
            // 0x4171c0 推进 +0x2c798 并把返回值写入 scale；每 3 帧一帧回到 1.0（闪烁）
            scale = step_interp_float(scale_interp, game_speed);
            if (state_timer.current == (state_timer.current / 3) * 3) vm.scale = {1.0f, 1.0f};
            else vm.scale = {scale, scale};
        } else {
            vm.scale = {scale, scale};
        }
        vm.flags_lo |= 8u;
        const float s = scale;
        rect_min = {pos.x - s * hit_radius.x, pos.y - hit_radius.y * s, pos.z - hit_radius.z * s};
        rect_max = {pos.x + hit_radius.x * s, pos.y + hit_radius.y * s, pos.z + hit_radius.z * s};
        item_rect_min = {pos.x - s * item_radius.x * 0.5f, pos.y - s * item_radius.y * 0.5f, pos.z - s * item_radius.z * 0.5f};
        item_rect_max = {pos.x + s * item_radius.x * 0.5f, pos.y + s * item_radius.y * 0.5f, pos.z + s * item_radius.z * 0.5f};
        graze_rect_min = {pos.x - s * graze_radius.x, pos.y - graze_radius.y * s, pos.z - graze_radius.z * s};
        graze_rect_max = {pos.x + s * graze_radius.x, pos.y + graze_radius.y * s, pos.z + graze_radius.z * s};
        item_rect2_min = {pos.x - s * item_radius.x, pos.y - item_radius.y * s, pos.z - item_radius.z * s};
        item_rect2_max = {pos.x + s * item_radius.x, pos.y + item_radius.y * s, pos.z + item_radius.z * s};
    }

    state_timer.tick(game_speed);
    timer2.tick(game_speed);
    timer3.tick(game_speed);

    const bool may_shoot = !world.dialogue && world.enemy_count != 0 && !world.stage_clear && timer3.current > 0x13 &&
                           (flags & kFlagNoShot) == 0 && (flags & kFlagScaled) == 0;
    if (may_shoot) {
        shoot(game_input, game_speed);
    } else {
        reset_shot_timers();
        shot_misc = 0;
        shot_misc_b = 0;
        // DAT_004d9e2c 音效队列：登记 0x1e / 0x37 停止标记——Sound 接入时补
    }
    // 0x4456d0：自机弹推进（池中无活动弹）
}

void Player::shoot(const InputState& game_input, float game_speed) {
    // 0x4455d0：射击计时；发弹本体 0x440d50（计时器置 0）/ 0x445470（按 .sht 组出弹）随射击系统接入
    if (state != kNormal) {
        shot_misc = 0;
        shot_misc_b = 0;
        return;
    }
    const bool shot_held = (game_input.held & InputState::kShot) != 0;
    bool skip_main = false;
    if (shot_timer.current < 0) {
        if (!shot_held) skip_main = true;
        else {
            if (shot_timer2.current < 0) shot_timer2.set(0);
            shot_timer.set(0);  // 0x440d50(this, 0)
        }
    }
    if (!skip_main) {
        // if (shot_timer.current != shot_timer.previous) 0x445470(this, shot_timer.current, shot_timer2.current)
        if (shot_timer.current < 0xe) shot_timer.tick(game_speed);
        else if (!shot_held) shot_timer.set(-1);
        else shot_timer.rewind(0xe, game_speed);
    }
    if (shot_timer2.current < 0) return;
    if (shot_timer2.current > 0x76) {
        if (!shot_held) shot_timer2.set(-1);
        else shot_timer2.rewind(0x77, game_speed);
        return;
    }
    shot_timer2.tick(game_speed);
}

void Player::prepare_draw() {
    // 0x443730（绘制链 0x1d）：state != 2 时把 pos 抄进 VM entity_offset，flags_hi = (hi & ~0x80000) | 0x40000，再 0x468490 绘制
    if (state == kRespawnWait) return;
    vm.entity_offset = pos;
    vm.flags_hi = (vm.flags_hi & 0xfff7ffffu) | 0x40000u;
}

}  // namespace th16
