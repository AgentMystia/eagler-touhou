// th16 Enemy / EnemyManager 还原——见 Enemy.hpp 锚点。
#include "Enemy.hpp"
#include "AnmManager.hpp"
#include "Effect.hpp"
#include "Item.hpp"
#include "Spellcard.hpp"
#include "GameGlobals.hpp"
#include "Player.hpp"
#include <cmath>
#include <cstring>

namespace th16 {
namespace {
constexpr float kSentinel = -999999.0f;
constexpr u32 kFlagMirror = 0x80000;
constexpr u32 kFlagBossLife = 0x40000000;
constexpr u32 kFlagReentry = 0x40000;
constexpr u32 kFlagAlive = 0x10000;
constexpr float kPi = 3.1415927410125732f;
constexpr float kHalfPi = 1.5707963705062866f;  // 0x494534
}  // namespace

const char* Enemy::routine_name() const noexcept {
    if (!program || primary.routine < 0 || u32(primary.routine) >= program->definitions.size()) return nullptr;
    return program->subroutine(u32(primary.routine)).name.c_str();
}

// 0x443840：自机相对 from 的方向角；重合时返回 π/2（DAT_00494534）
float Enemy::angle_to_player(const Vec3& from) const noexcept {
    if (player == nullptr) return kHalfPi;
    const float dy = player->pos.y - from.y;
    const float dx = player->pos.x - from.x;
    if (dy == 0.0f && dx == 0.0f) return kHalfPi;
    return atan2_f(dy, dx);
}

// 0x417580：按 boss_ids[slot] 查敌机（未命中时原版返回最后遍历到的敌机，此处与 find_id 的 nullptr 口径一致）
Enemy* Enemy::boss(int slot) const {
    if (manager == nullptr || manager->boss_ids[slot] == 0) return nullptr;
    return manager->find_id(manager->boss_ids[slot]);
}

bool Enemy::integer(i32 var, i32& value) {
    switch (var) {
        case -10000: value = rng ? signed_bits(rng->next32() & 0x7fffffffu) : 0; return true;
        case -9999: value = rng ? truncate_int(rng->unit()) : 0; return true;
        case -9997: case -9977: value = truncate_int(pos.x); return true;
        case -9996: case -9976: value = truncate_int(pos.y); return true;
        case -9995: case -9975: value = truncate_int(motion1.pos.x); return true;
        case -9994: case -9974: value = truncate_int(motion1.pos.y); return true;
        case -9993: case -9973: value = truncate_int(motion2.pos.x); return true;
        case -9992: case -9972: value = truncate_int(motion2.pos.y); return true;
        case -9991: case -9965: value = truncate_int(player ? player->pos.x : 0); return true;
        case -9990: case -9964: value = truncate_int(player ? player->pos.y : 0); return true;
        case -9988: value = life_timer.current; return true;
        case -9987: value = rng ? truncate_int(rng->signed_unit()) : 0; return true;
        case -9986: value = (flags >> 24) & 1; return true;
        case -9985: value = int_regs[0]; return true;
        case -9984: value = int_regs[1]; return true;
        case -9983: value = int_regs[2]; return true;
        case -9982: value = int_regs[3]; return true;
        case -9981: value = truncate_int(float_regs[0]); return true;
        case -9980: value = truncate_int(float_regs[1]); return true;
        case -9979: value = truncate_int(float_regs[2]); return true;
        case -9978: value = truncate_int(float_regs[3]); return true;
        case -9971: value = truncate_int(motion1.angle); return true;   // +0x12b0
        case -9970: value = truncate_int(motion2.angle); return true;   // +0x12f4
        case -9969: value = truncate_int(motion1.speed); return true;   // +0x12ac
        case -9968: value = truncate_int(motion2.speed); return true;   // +0x12f0
        case -9967: value = truncate_int(motion1.radius); return true;  // +0x12b4
        case -9966: value = truncate_int(motion2.radius); return true;  // +0x12f8
        case -9963: { const Enemy* b = boss(0); value = b ? truncate_int(b->pos.x) : 0; return true; }
        case -9962: { const Enemy* b = boss(0); value = b ? truncate_int(b->pos.y) : 0x80; return true; }
        case -9958: value = truncate_int(atan2_f(delta.y, delta.x)); return true;  // 0x4052a0(+0x1288, +0x1284)
        case -9931: value = manager ? i32(manager->last_id) : 0; return true;     // DAT_004a6dc0+0x94
        case -9960: value = globals ? globals->rank : 0; return true;  // DAT_004a57bc
        case -9959: value = globals ? globals->difficulty : 0; return true;
        case -9954: value = life; return true;
        case -9953: value = globals && globals->difficulty == 0; return true;
        case -9952: value = globals && globals->difficulty == 1; return true;
        case -9951: value = globals && globals->difficulty == 2; return true;
        case -9950: value = globals && globals->difficulty == 3; return true;
        case -9949: case -9948: case -9947:
            value = manager ? manager->counters[var + 9949] : 0;
            return true;
        case -9946: value = manager ? manager->enemy_count : 0; return true;
        case -9945: value = globals ? globals->shot + globals->shot_aux : 0; return true;
        case -9943: case -9942: case -9941: case -9940:
            value = int_regs[var + 9943];
            return true;
        case -9935: case -9934: case -9933: case -9932:
            value = truncate_int(temp_regs[var + 9935]);
            return true;
        case -9926: case -9925: case -9924: case -9923:
            value = manager ? manager->global_ints[var + 9926] : 0;
            return true;
        case -9922: case -9921: case -9920: case -9919:
        case -9918: case -9917: case -9916: case -9915:
            value = manager ? truncate_int(manager->global_floats[var + 9922]) : 0;
            return true;
        case -9914: value = i32(id); return true;
        case -9907: value = globals ? globals->replay_slot : 0; return true;
        case -9906: value = (flags >> 19) & 1; return true;
        case -9905: value = globals ? globals->stage_frame : 0; return true;
        case -9903: value = globals ? globals->season : 0; return true;
        case -9902: value = globals ? globals->power : 0; return true;
        default: error = "Enemy integer var " + std::to_string(var); return false;
    }
}

bool Enemy::floating(i32 var, float& value) {
    switch (var) {
        case -10000: value = rng ? float(rng->next32() & 0x7fffffffu) : 0; return true;
        case -9999: value = rng ? rng->unit() : 0; return true;
        case -9998: value = rng ? float(rng->signed_unit() * kPi) : 0; return true;
        case -9997: case -9977: value = pos.x; return true;
        case -9996: case -9976: value = pos.y; return true;
        case -9995: case -9975: value = motion1.pos.x; return true;
        case -9994: case -9974: value = motion1.pos.y; return true;
        case -9993: case -9973: value = motion2.pos.x; return true;
        case -9992: case -9972: value = motion2.pos.y; return true;
        case -9991: case -9965: value = player ? player->pos.x : 0; return true;
        case -9990: case -9964: value = player ? player->pos.y : 0; return true;
        case -9988: value = life_timer.current_f; return true;
        case -9987: value = rng ? rng->signed_unit() : 0; return true;
        case -9981: value = float_regs[0]; return true;
        case -9980: value = float_regs[1]; return true;
        case -9979: value = float_regs[2]; return true;
        case -9978: value = float_regs[3]; return true;
        case -9939: case -9938: case -9937: case -9936:
            value = float_regs[var + 9939];
            return true;
        case -9935: case -9934: case -9933: case -9932:
            value = temp_regs[var + 9935];
            return true;
        case -9922: case -9921: case -9920: case -9919:
        case -9918: case -9917: case -9916: case -9915:
            value = manager ? manager->global_floats[var + 9922] : 0;
            return true;
        case -9989: value = angle_to_player(pos); return true;          // 0x443840(+0x1250)
        case -9971: value = motion1.angle; return true;
        case -9970: value = motion2.angle; return true;
        case -9969: value = motion1.speed; return true;
        case -9968: value = motion2.speed; return true;
        case -9967: value = motion1.radius; return true;
        case -9966: value = motion2.radius; return true;
        case -9963: { const Enemy* b = boss(0); value = b ? b->pos.x : 0.0f; return true; }
        case -9962: { const Enemy* b = boss(0); value = b ? b->pos.y : 128.0f; return true; }
        case -9958: value = atan2_f(delta.y, delta.x); return true;
        case -9956: value = angle_to_player(motion1.pos); return true;  // 0x443840(+0x1294)
        case -9955: value = angle_to_player(motion2.pos); return true;  // 0x443840(+0x12d8)
        case -9931: value = manager ? float(double(manager->last_id)) : 0.0f; return true;  // 无符号 → double → float
        default: {
            i32 i = 0;
            if (!integer(var, i)) return false;
            value = float(i);
            return true;
        }
    }
}

i32* Enemy::integer_destination(i32 var) {
    switch (var) {
        case -9985: case -9984: case -9983: case -9982:
            return &int_regs[var + 9985];
        case -9949: case -9948: case -9947:
            return manager ? &manager->counters[var + 9949] : nullptr;
        case -9943: case -9942: case -9941: case -9940:
            return &int_regs[var + 9943];
        case -9926: case -9925: case -9924: case -9923:
            return manager ? &manager->global_ints[var + 9926] : nullptr;
        default: error = "Enemy integer dest " + std::to_string(var); return nullptr;
    }
}

float* Enemy::float_destination(i32 var) {
    switch (var) {
        case -9997: case -9977: return &pos.x;
        case -9996: case -9976: return &pos.y;
        case -9995: case -9975: return &motion1.pos.x;
        case -9994: case -9974: return &motion1.pos.y;
        case -9993: case -9973: return &motion2.pos.x;
        case -9992: case -9972: return &motion2.pos.y;
        case -9981: case -9980: case -9979: case -9978:
            return &float_regs[var + 9981];
        case -9939: case -9938: case -9937: case -9936:
            return &float_regs[var + 9939];
        case -9935: case -9934: case -9933: case -9932:
            return &temp_regs[var + 9935];
        case -9922: case -9921: case -9920: case -9919:
        case -9918: case -9917: case -9916: case -9915:
            return manager ? &manager->global_floats[var + 9922] : nullptr;
        case -9969: return &motion1.angle;
        case -9968: return &motion2.angle;
        case -9967: return &motion1.speed;
        case -9966: return &motion2.speed;
        default: error = "Enemy float dest " + std::to_string(var); return nullptr;
    }
}

// 0x474430：新建上下文（0x41a400）、头插异步线程链（主上下文节点 +0x11fc 之后），
// 再以 0x471db0 在新上下文上建立调用帧并传入 spawn 指令的参数
bool Enemy::spawn(EclContext& source, i32 id, u32 skipped_arguments) {
    auto ctx = std::make_unique<EclContext>();
    ctx->variables = this;
    ctx->threads = this;
    ctx->commands = this;
    ctx->game_rng = rng;
    ctx->difficulty = source.difficulty;
    ctx->interpolation_rate = source.interpolation_rate;
    ctx->thread_id = id;
    ctx->program = program;
    if (!source.instruction || !program) return false;
    if (!source.call_into(*ctx, skipped_arguments)) { error = source.error; return false; }
    async_threads.push_front(std::move(ctx));
    return true;
}

EclContext* Enemy::find(i32 id) {
    for (auto& t : async_threads) if (t && t->thread_id == id) return t.get();
    return nullptr;
}

void Enemy::terminate_children() {
    for (auto& t : async_threads) if (t) t->instruction_offset = -1;
}

void Enemy::reset_context(EclContext& ctx, i32 routine) {
    const u8 diff = primary.difficulty ? primary.difficulty : 0xff;
    ctx = EclContext{};
    ctx.variables = this;
    ctx.threads = this;
    ctx.commands = this;
    ctx.game_rng = rng;
    ctx.program = program;
    ctx.difficulty = diff;
    if (routine >= 0) ctx.bind(*program, routine);
}

bool Enemy::init(EnemyManager& mgr, EclProgram& prog, const char* sub, const EnemySpawnArgs& args,
                 GameGlobals& g, Player* pl, Rng& r, u8 difficulty) {
    manager = &mgr;
    program = &prog;
    globals = &g;
    player = pl;
    rng = &r;
    bullets = mgr.bullets;
    items = mgr.items;
    effects = mgr.effects;
    std::memset(vm_ids, 0, sizeof vm_ids);
    for (int i = 0; i < kVmSyncSlots; ++i) { vm_offsets[i] = Vec3{}; vm_parent_slot[i] = -1; }
    anm_slot = anm_slot_main = main_script = main_script_base = sprite_direction = 0;
    anm_layer = anm_layer_offset = 0;
    delta = Vec3{};
    vm_rotation = 0;
    death_anm_slot = 0;
    death_script = -1;
    std::memset(static_cast<void*>(shooters), 0, sizeof shooters);
    for (int i = 0; i < kShooterSlots; ++i) { shooter_offset[i] = Vec3{}; shooter_pos2[i] = Vec3{}; shooter_ex_counter[i] = 0; }
    safe_radius_sq = 0;
    id = mgr.next_id;
    mgr.last_id = id;  // DAT_004a6dc0+0x94（ECL -9931）
    mgr.next_id = mgr.next_id + 1;
    if (mgr.next_id == 0) mgr.next_id = 1;
    flags = 0;
    if (args.mirror) flags |= kFlagMirror;
    if (args.life >= 1000) flags |= kFlagBossLife;
    pos = args.pos;
    motion1 = MotionState{};
    motion2 = MotionState{};
    motion1.pos = args.pos;
    hitbox = {24.0f, 24.0f, 0.0f};
    hurtbox = {24.0f, 24.0f, 0.0f};
    life = truncate_int(args.life);
    life_max = life;
    life_display = 0;
    phase_life = 0;
    life_budget = 0;  // 0x41aa70 不预置血量池（op 511 / 522 族才写 life × 7）
    life_flags = 0;
    damage_accum = 0;
    damage_rate = 1.0f;
    season_item_step = season_item_next = 0;
    hit_timer = Timer{};
    no_hit_timer = Timer{};
    score_bonus = score_bonus_spell = 0;
    bomb_script = bomb_script_restore = 0;
    for (auto& it : interrupts) it = EnemyInterrupt{};
    int_regs[0] = 1;  // 0x41aa70：puVar6[0x1498] = 1
    invuln.set(2);
    life_timer.set(0x3c);
    season_drop_timer.set(0x3c);
    season_drop_duration = 0x3c;
    season_drop_max = 10;        // 0x41aa70：puVar6[0x1477] = 10
    season_drop_min = 1;
    drop_item_type = args.item;  // 0x41aa70：+0x519c = 生成参数 +0x10
    std::memset(drop_counts, 0, sizeof drop_counts);
    drop_radius = {32.0f, 32.0f};
    death_sub.clear();
    death_sfx = i32((id & 1u) + 3u);
    primary.difficulty = difficulty;
    const i32 routine = prog.find_index(sub);
    if (routine < 0) { error = std::string("missing sub ") + sub; return false; }
    reset_context(primary, routine);
    // 0x41aa70 → 0x41d1e0：立即跑一帧；0x40000 留给管理器清除（从而再隔一帧才第二次真正更新）
    update(1.0f);
    return alive;
}

bool Enemy::update(float game_speed) {
    if (!alive) return false;
    // 0x41d2e0：若 0x40000 已置位则整帧跳过（生成后首个管理器回合只清标志）
    if ((flags & kFlagReentry) != 0) return true;
    flags |= kFlagReentry;

    // 0x41bb50：先推进运动插值器（角度 +0x3c8 / 速度 +0x3f8 / 位置 +0x2f8），再算速度与积分
    if (angle_interp.end_time && (motion1.mode & 0xf) != 2 && (motion1.mode & 0xf) != 3)
        motion1.angle = wrap_angle(step_interp_float(angle_interp, game_speed));
    if (speed_interp.end_time)
        motion1.speed = step_interp_float(speed_interp, game_speed);

    if (pos_interp.end_time == 0) {
        motion1.update_velocity(game_speed);
    } else {
        const Vec3 target = step_interp_vec3(pos_interp, game_speed);
        motion1.velocity.x = target.x - motion1.pos.x;
        motion1.velocity.y = target.y - motion1.pos.y;
        motion1.velocity.z = target.z - motion1.pos.z;
    }
    motion1.integrate(game_speed);
    motion2.update_velocity(game_speed);
    motion2.integrate(game_speed);
    // 0x41c1f0：本帧位移存 EnemyInf+0x78（朝向切换与 0x80 位 VM 的朝向角用）
    delta.x = (motion1.pos.x + motion2.pos.x) - pos.x;
    delta.y = (motion1.pos.y + motion2.pos.y) - pos.y;
    delta.z = (motion1.pos.z + motion2.pos.z) - pos.z;
    pos.x = floor_f((pos.x + delta.x) * 100.0f) / 100.0f;
    pos.y = floor_f((pos.y + delta.y) * 100.0f) / 100.0f;
    pos.z = pos.z + delta.z;
    if ((flags & 0x20000u) != 0) {
        const float hx = bound_size.x * 0.5f, hy = bound_size.y * 0.5f;
        if (pos.x < bound_center.x - hx) pos.x = bound_center.x - hx;
        else if (pos.x > bound_center.x + hx) pos.x = bound_center.x + hx;
        if (pos.y < bound_center.y - hy) pos.y = bound_center.y - hy;
        else if (pos.y > bound_center.y + hy) pos.y = bound_center.y + hy;
        motion1.pos.x = pos.x - motion2.pos.x;
        motion1.pos.y = pos.y - motion2.pos.y;
        motion1.pos.z = pos.z - motion2.pos.z;
    }

    // 0x41bb50 尾：朝向切换 + 从主 VM 回读精灵宽高（+0x3f58/+0x3f5c，绑定前为 0），再判进入场地与出界销毁
    sync_sprite_direction();
    const float half_w = sprite_size.x * 0.5f, half_h = sprite_size.y * 0.5f;
    if (pos.x + half_w < -192.0f || 192.0f < pos.x - half_w) {
        if ((flags & 0x10004u) == 0x10000u) { alive = false; return false; }
    } else if (pos.y + half_h < 0.0f || 448.0f < pos.y - half_h) {
        if ((flags & kFlagAlive) != 0 && (flags & 8u) == 0) { alive = false; return false; }
    } else {
        flags |= kFlagAlive;
    }

    // 0x473bc0：先取下一节点再执行当前上下文——执行中头插的新线程（0x474430）本帧不跑
    primary.interpolation_rate = game_speed;
    auto it = async_threads.begin();
    int r = primary.step(game_speed);
    if (r == -2) { error = primary.error; alive = false; return false; }
    if (r == -1) { alive = false; return false; }
    while (it != async_threads.end()) {
        auto next = std::next(it);
        auto& t = *it;
        if (!t || t->instruction_offset < 0) { async_threads.erase(it); it = next; continue; }
        t->interpolation_rate = game_speed;
        const int ar = t->step(game_speed);
        if (ar == -2) { error = t->error; alive = false; return false; }
        if (ar == -1) async_threads.erase(it);
        it = next;
    }

    if (resolve_hits(game_speed) != 0) { alive = false; return false; }
    sync_vm_offsets();

    if (invuln.current > 0) invuln.untick(game_speed);
    if (no_hit_timer.current > 0) no_hit_timer.untick(game_speed);
    life_timer.tick(game_speed);
    if (season_drop_timer.current > 0) season_drop_timer.untick(game_speed);
    // 注意：不在这里清除 kFlagReentry——由 EnemyManager::update 清除（对齐 0x41b3d0）
    return true;
}

AnmFile* Enemy::anm_file(i32 index) const noexcept {
    if (manager == nullptr || manager->animations == nullptr) return nullptr;
    if (index == 0) return manager->animations->file(7);
    if (index == 1) return manager->animations->file(8);
    return index >= 2 ? manager->animations->file(8 + index) : nullptr;
}

AnmVm* Enemy::vm_at(int slot) noexcept {
    if (slot < 0 || slot >= kVmSlots || manager == nullptr || manager->animations == nullptr) return nullptr;
    AnmVm* vm = manager->animations->find(vm_ids[slot]);
    if (vm == nullptr) vm_ids[slot] = 0;
    return vm;
}

AnmVm* Enemy::rebind_vm(int slot, i32 source, i32 script, bool full) {
    if (slot < 0 || slot >= kVmSlots || manager == nullptr || manager->animations == nullptr) return nullptr;
    AnmManager& anm = *manager->animations;
    anm.kill(vm_ids[slot]);
    vm_ids[slot] = 0;
    AnmFile* file = anm_file(source);
    if (file == nullptr) return nullptr;
    vm_ids[slot] = anm.spawn_front(*file, script, anm_layer_offset + 7);
    AnmVm* vm = vm_at(slot);
    if (vm == nullptr) return nullptr;
    if (!full) return vm;
    if (slot == 0) {
        sprite_size.x = vm->sprite_size.y * vm->scale.y;
        sprite_size.y = vm->sprite_size.x * vm->scale.x;
    }
    if ((flags & 0x20u) != 0) AnmManager::hide_tree(*vm);
    return vm;
}

// 0x41bb50 尾段
void Enemy::sync_sprite_direction() {
    if (manager == nullptr || manager->animations == nullptr) return;
    AnmManager& anm = *manager->animations;
    if ((flags & 0x100000u) != 0) {
        const i32 dir = delta.x < -0.03f ? -1 : (0.03f < delta.x ? 1 : 0);
        if (sprite_direction != dir) {
            i32 step = 0;
            if (sprite_direction == -1) step = dir != 0 ? 2 : 3;
            else if (sprite_direction == 0) step = dir != -1 ? 2 : 1;
            else if (sprite_direction == 1) step = dir == 0 ? 4 : 1;
            AnmVm* old = vm_at(0);
            Vec3 offset{};
            if (old != nullptr) {
                offset = old->pos;
                anm.kill(vm_ids[0]);
                vm_ids[0] = 0;
            }
            if (AnmFile* file = anm_file(anm_slot_main)) {
                vm_ids[0] = anm.spawn_vm(*file, main_script_base + step, anm_layer_offset + 7, offset, 0.0f, 8, false);
            }
            sprite_direction = dir;
        }
    }
    if (AnmVm* vm = vm_at(0)) {
        sprite_size.x = std::fabs(vm->sprite_size.y * vm->scale.y);
        sprite_size.y = std::fabs(vm->sprite_size.x * vm->scale.x);
    }
}

// 0x41d2e0 后段
void Enemy::sync_vm_offsets() {
    if (manager == nullptr || manager->animations == nullptr) return;
    if ((flags & 0x4000000u) != 0) {
        for (int i = 0; i < kVmSyncSlots; ++i) {
            if (AnmVm* vm = manager->animations->find(vm_ids[i])) vm->entity_offset = pos;
        }
        return;
    }
    for (int i = 0; i < kVmSyncSlots; ++i) {
        AnmVm* vm = vm_at(i);
        if (vm == nullptr) continue;
        Vec3 offset{vm_offsets[i].x + pos.x, vm_offsets[i].y + pos.y, pos.z + vm_offsets[i].z};
        if (vm_parent_slot[i] >= 0) {
            if (AnmVm* parent = vm_at(vm_parent_slot[i])) {
                offset.x = parent->pos.x + offset.x;
                offset.y = parent->pos.y + offset.y;
                offset.z = parent->pos.z + offset.z;
            }
        }
        vm->entity_offset = offset;
        if ((vm->flags_hi & 0x80u) != 0) {
            const float angle = atan2_f(delta.y, delta.x);
            vm->flags_lo |= AnmVm::kRotationDirty;
            vm->rotation.z = angle;
            vm_rotation = angle;
        }
    }
}

// 0x41c330 尾段（主 VM 闪烁 / 受击音效）
void Enemy::update_hit_flash() {
    AnmVm* vm = vm_at(0);
    if (vm == nullptr) return;
    auto flash = [&](u32 bgra) {
        std::memcpy(vm->color2, &bgra, 4);
        vm->flags_lo = (vm->flags_lo & ~0x40000u) | 0x20000u;
    };
    if (hit_flash_timer != 0) {
        vm->flags_lo &= ~0x60000u;
        --hit_flash_timer;
        return;
    }
    const u32 spell = manager != nullptr && manager->spellcard != nullptr ? manager->spellcard->flags : 0u;
    // 0x40800000 位（Boss 家族）在非符卡取得阶段按血量池阈值闪蓝
    auto low_life_flash = [&](i32 spell_limit, i32 normal_limit) {
        if ((flags & 0x40800000u) == 0 || (spell & 9u) == 9u) return false;
        return (spell & 1u) != 0 ? life_display < spell_limit : life_display < normal_limit;
    };
    if ((flags & 0x80000000u) != 0) {
        if (life_timer.current % 4 == 0) flash(0xffff00ffu);
        else vm->flags_lo &= ~0x60000u;
    }
    if ((flags & 0x200000u) == 0 || (flags & 0x2000u) != 0) {
        if (life_timer.current % 4 == 0) {
            if (low_life_flash(100, 500)) flash(0xff0000ffu);
        } else {
            vm->flags_lo &= ~0x60000u;
        }
        return;
    }
    flash(0xff0000ffu);
    hit_flash_timer = 4;
    if (manager == nullptr) return;
    if (hit_sfx >= 0) manager->sfx_queue.push_back(hit_sfx);
    else manager->sfx_queue.push_back(low_life_flash(200, 900) ? 0x23 : 0x22);
}

// 0x424e40
void Enemy::set_interrupt(int slot, i32 life_threshold, i32 time_threshold, const char* sub) {
    if (slot < 0 || slot >= kInterruptSlots) return;
    EnemyInterrupt& it = interrupts[slot];
    it.life_threshold = life_threshold;
    it.time_threshold = time_threshold;
    if (sub != nullptr) { it.sub = sub; it.timeout_sub = sub; }
    else { it.sub.clear(); it.timeout_sub.clear(); }
}

// 0x424f00
const std::string* Enemy::life_interrupt() {
    const i32 current = life;
    life_display = current;
    phase_life = 0;
    for (int i = 0; i < kInterruptSlots; ++i) {
        EnemyInterrupt& it = interrupts[i];
        if (it.life_threshold < 0) continue;
        life_display = current - it.life_threshold;
        phase_life = it.life_threshold;
        if (it.life_threshold < current) return nullptr;
        if (score_bonus != 0 && globals != nullptr && score_bonus_spell == globals->spell_id) {
            globals->enemy_bonus += score_bonus;
            score_bonus = 0;
        }
        life = it.life_threshold;
        it.life_threshold = -1;
        life_timer.reset();
        flags &= ~0x1000000u;
        return &it.sub;
    }
    return nullptr;
}

// 0x425010
const std::string* Enemy::time_interrupt() {
    int slot = 0;
    for (;; ++slot) {
        if (slot >= kInterruptSlots) return nullptr;
        if (interrupts[slot].life_threshold >= 0 && interrupts[slot].time_threshold >= 1) break;
    }
    EnemyInterrupt& it = interrupts[slot];
    // Boss 位（0x800000）：Gui+0x1d0/+0x1d4 的剩余秒数显示由 Gui 接入时补（纯显示）
    if (life_timer.current < it.time_threshold) return nullptr;
    life = it.life_threshold;
    it.life_threshold = -1;
    life_timer.reset();
    flags |= 0x1000000u;
    Spellcard* spell = manager != nullptr ? manager->spellcard : nullptr;
    const u32 sflags = spell != nullptr ? spell->flags : 0u;
    if ((sflags & Spellcard::kSurvival) == 0) {
        flags &= ~0x1000000u;
        if (spell != nullptr) {
            spell->flags |= Spellcard::kTimedOut;
            if ((spell->flags & Spellcard::kActive) != 0) {
                if (spell->timer.current > 0x3b) {
                    spell->bonus = 0;
                    spell->flags &= ~(Spellcard::kBonusAvailable | Spellcard::kBombUsed);
                    manager->counters[2] = 0;
                    score_bonus = 0;
                    return &it.timeout_sub;
                }
                if (manager->bomb_active) spell->flags |= Spellcard::kBombUsed;
            }
        }
        if (manager != nullptr) manager->counters[2] = 0;
    } else if ((sflags & (Spellcard::kSurvival | Spellcard::kActive)) == (Spellcard::kSurvival | Spellcard::kActive)) {
        if (globals != nullptr) globals->enemy_bonus += score_bonus;
    }
    score_bonus = 0;
    return &it.timeout_sub;
}

// 0x41a4f0 + 0x41a5a0 + 0x474740/0x474890 + 0x473bc0
bool Enemy::switch_routine(const std::string& sub, float game_speed) {
    if (program == nullptr) return true;
    const i32 routine = program->find_index(sub.c_str());
    async_threads.clear();
    if (routine < 0) { error = "missing interrupt sub " + sub; return false; }
    reset_context(primary, routine);
    primary.interpolation_rate = game_speed;
    const int r = primary.step(game_speed);
    if (r == -2) { error = primary.error; return false; }
    return r == 0;
}

// 0x41c330
int Enemy::resolve_hits(float game_speed) {
    if (manager == nullptr) return 0;
    Spellcard* spell = manager->spellcard;
    const u32 sflags = spell != nullptr ? spell->flags : 0u;
    AnmManager* anm = manager->animations;

    // Bomb / 季节解放期间把主 VM 换成 bomb_script（op 546），结束后换回
    const bool burst = manager->bomb_active || manager->release_active;
    auto replace_main = [&](i32 script) {
        // 0x46f440：旧 VM 停止（flags_lo bit0 清、ins_offset = -1），同文件 script 在层 -1 新建
        main_script_base = script;
        AnmVm* vm = vm_at(0);
        if (vm == nullptr || anm == nullptr) return;
        AnmFile* file = anm->file(vm->file_slot);
        vm->flags_lo &= ~1u;
        vm->ins_offset = -1;
        if (file != nullptr) vm_ids[0] = anm->spawn(*file, script, -1);
    };
    if ((flags & 0x10000000u) != 0 && burst && (flags & 0x20000000u) == 0) {
        replace_main(bomb_script);
        flags |= 0x20000001u;
    } else if (!burst && (flags & 0x20000000u) != 0) {
        replace_main(bomb_script_restore);
        flags &= ~0x20000001u;
    }

    // flags & 0x800：被炸弹类伤害源碰到即死（只判定不结算）
    if ((flags & 0x800u) != 0 && player != nullptr && anm != nullptr && globals != nullptr) {
        bool bomb_hit = false;
        const i32 probe = player->apply_damage_sources(*anm, *globals, pos, hitbox.x * 0.5f, id, &hit_pos, &bomb_hit, true);
        if (probe != 0 && bomb_hit) { die(game_speed); return 1; }
    }

    // 0x425010：时间阶段中断
    if (const std::string* sub = time_interrupt()) {
        if (!switch_routine(*sub, game_speed)) return -1;
    }

    const u32 before = flags;
    flags &= ~0x200000u;
    bool bomb_hit = false;
    if ((before & 0x21u) == 0) {
        i32 dmg = 0;
        if (hitbox.x > 0.0f && player != nullptr && anm != nullptr && globals != nullptr) {
            dmg = player->apply_damage_sources(*anm, *globals, pos, hitbox.x * 0.5f, id, &hit_pos, &bomb_hit);
            dmg = i32(float(dmg) * player->scale2);
        }
        // +0x4520 伤害回调 / +0x3fe0 待加伤害：无 ECL 写入路径，恒 0
        if (player != nullptr && (player->state == 2 || player->state == 0)) dmg /= 5;
        i32 applied = manager->dialogue_active ? 0 : dmg;
        if (applied > 0) {
            if (!bomb_hit) {
                manager->damage_dealt += applied;
            } else if (applied < life) {
                manager->damage_dealt_bomb += applied;
            } else {
                manager->damage_dealt_bomb += ((applied - life) >> 2) + life;
            }
        }
        if (manager->bomb_active && damage_rate < 1.0f) {
            if (applied != 0 && damage_rate <= 0.0f) manager->sfx_queue.push_back(0x24);
            applied = i32(float(applied) * damage_rate);
        }
        if (applied != 0) {
            if ((sflags & (Spellcard::kActive | Spellcard::kDamageSuppressed)) ==
                (Spellcard::kActive | Spellcard::kDamageSuppressed))
                applied /= 0x1e;
            if ((flags & 0x10u) == 0 && invuln.current < 1) {
                // 0x41a6d0：符卡期间（life_flags & 1）按 7 倍血量池折算
                if ((life_flags & 1u) != 0) {
                    life_budget -= applied;
                    life = (life_budget - phase_life * 7) / 7 + phase_life;
                } else {
                    life -= applied;
                }
            } else {
                damage_accum += applied;
            }
            // op 1001：每累计 season_item_step 伤害放一个季节道具（visual_rng：unit + angle）
            if (season_item_step > 0 && season_item_next < damage_accum && rng != nullptr) {
                do {
                    season_item_next += season_item_step;
                    const float speed = float(double(rng->unit()) + double(1.2f));   // x87：unit + 1.2f
                    const float angle = rng->angle();
                    if (items != nullptr) items->spawn(0x10, pos, angle, speed, 0);
                } while (season_item_next < damage_accum);
            }
            hit_timer.set(0x1e);
            if (const std::string* sub = life_interrupt()) {
                if (!switch_routine(*sub, game_speed)) return -1;
            }
            if (life < 1 && ((flags >> 7) & 1u) == 0) { die(game_speed); return 1; }
            flags |= 0x200000u;
        }
    }
    if ((life_flags & 2u) != 0) { die(game_speed); return 1; }

    // 体碰撞（0x4439e0；flags & 0x1000 的矩形体碰撞 0x443af0 无 ECL 设置路径）与擦弹（flags & 0x200）
    if ((flags & 0x22u) == 0 && no_hit_timer.current < 1 && (flags & 0x4000000u) == 0 && player != nullptr) {
        int hit = 0;
        if (anm != nullptr)
            hit = player->collide_body(*anm, anm_file(1), pos, hurtbox.x * 0.5f, manager->dialogue_active, false);
        if ((flags & 0x200u) != 0 && hit == 2 && life_timer.current % 6 == 0 && bullets != nullptr)
            bullets->graze(player->pos);
    }

    update_hit_flash();
    if (hit_timer.current > 0) hit_timer.untick(game_speed);
    return 0;
}

// 0x41d700
void Enemy::scatter_items() {
    if (rng == nullptr) return;
    auto request = [&](i32 type, const Vec3& at, float angle, float speed) {
        if (items != nullptr) items->spawn(type, at, angle, speed, 0);
    };
    float angle = float(double(rng->signed_unit()) * double(kPi));
    for (int index = 0; index < 16; ++index) {
        if (index == 15) {
            for (i32 i = 0; i < season_drop_max; ++i) {
                const float speed = float(double(rng->unit()) * double(1.9f) + double(0.2f));   // x87：unit × 1.9f + 0.2f
                const float a = rng->angle();
                request(0x10, pos, a, speed);
            }
            continue;
        }
        for (i32 i = 0; i < drop_counts[index]; ++i) {
            // 0x426240：椭圆半径上的点 × [0.5, 1) 随机缩放
            const float lx = float(std::cos(double(angle)) * double(drop_radius.x));
            const float ly = float(std::sin(double(angle)) * double(drop_radius.y));
            const float t = float(0.5 + double(rng->unit()) * 0.5);
            const Vec3 at{pos.x + t * lx, pos.y + ly * t, pos.z + 0.0f};
            request(index + 1, at, -1.5707964f, 2.2f);
            const float jitter = rng->signed_unit();
            angle = (angle + 1.5707964f) + (jitter * kPi) * 0.25f;
            for (int k = 0; angle > kPi && k <= 0x21; ++k) angle -= 6.2831855f;
            for (int k = 0; angle < -kPi && k <= 0x21; ++k) angle += 6.2831855f;
        }
    }
    std::memset(drop_counts, 0, sizeof drop_counts);
    season_drop_max = 0;
}

// 0x41a720
void Enemy::release_items() {
    if (drop_item_type != 0 && items != nullptr) items->spawn(drop_item_type, pos, -1.5707964f, 2.2f, 0);
    scatter_items();
    drop_item_type = 0;
}

void Enemy::settle_season_drop() {
    i32 count = season_drop_min;
    if (season_drop_timer.current >= 1 && season_drop_duration != 0) {
        count = ((season_drop_max - season_drop_min) * season_drop_timer.current) / season_drop_duration +
                season_drop_min;
    }
    season_drop_max = count;
}

// 0x41d520
void Enemy::die(float game_speed) {
    if (death_sfx >= 0 && manager != nullptr) manager->sfx_queue.push_back(death_sfx);
    if (death_script >= 0) {
        const float dx = hit_pos.x - pos.x, dy = hit_pos.y - pos.y;
        float angle = -1.5707964f;
        if (dx * dx + dy * dy >= 0.040000003f) angle = atan2_f(pos.y - hit_pos.y, pos.x - hit_pos.x);
        AnmFile* file = anm_file(death_anm_slot);
        if (file != nullptr && manager->animations != nullptr) {
            const u32 vid = manager->animations->spawn_effect(*file, death_script, 3, pos, angle);
            if (effects != nullptr) effects->register_id(vid);
        }
    }
    settle_season_drop();
    release_items();
    if (score_bonus > 0 && globals != nullptr && score_bonus_spell == globals->spell_id) {
        globals->enemy_bonus += score_bonus;
        score_bonus = 0;
    }
    if (!death_sub.empty() && program != nullptr) {
        const i32 routine = program->find_index(death_sub.c_str());
        async_threads.clear();
        if (routine >= 0) {
            reset_context(primary, routine);
            primary.interpolation_rate = game_speed;
            primary.step(game_speed);
        }
        death_sub.clear();
    }
}

// 0x41ba10：移出列表时（flags2 & 4 未置）杀掉全部绑定 VM 及其子树
void Enemy::destroy() {
    if (manager == nullptr || manager->animations == nullptr) return;
    for (u32& vid : vm_ids) {
        if (AnmVm* vm = manager->animations->find(vid)) AnmManager::kill_tree(*vm);
        vid = 0;
    }
}

int Enemy::execute(EclContext& ctx, u16 opcode) {
    auto iarg = [&](u32 n) {
        i32 v = 0;
        if (!ctx.integer(n, ctx.instruction->argument<i32>(n), v, false)) error = ctx.error;
        return v;
    };
    auto farg = [&](u32 n) {
        float v = 0;
        if (!ctx.floating(n, ctx.instruction->argument<float>(n), v, false)) error = ctx.error;
        return v;
    };
    auto fail = [&](const char* m) { error = m; ctx.error = m; return -2; };

    switch (opcode) {
        case 309: case 310: case 311: case 312:
            // 0x41dcb0 case 6：Boss 槽 0（0x417580(0)）仍在时不生成
            if (manager != nullptr && manager->boss_ids[0] != 0 && manager->find_id(manager->boss_ids[0]) != nullptr)
                return 0;
            [[fallthrough]];
        case 300: case 301: case 304: case 305: case 321: {
            // 0x423050
            if (!ctx.instruction || !manager) return fail("spawn: no manager");
            const u32 name_size = ctx.instruction->argument<u32>(0);
            const auto* bytes = reinterpret_cast<const u8*>(ctx.instruction);
            if (name_size == 0 || name_size % 4 || name_size > ctx.instruction->length - 20)
                return fail("spawn: bad name");
            const char* sub = reinterpret_cast<const char*>(bytes + 20);
            const u32 base = 20 + name_size;
            // 参数用 pop=false：引用栈槽由指令 stack_adjustment 统一清理（避免双重弹栈）
            auto read_f = [&](u32 arg_index) {
                float lit = 0;
                if (base + arg_index * 4 + 4 <= ctx.instruction->length)
                    std::memcpy(&lit, bytes + base + arg_index * 4, 4);
                float v = lit;
                ctx.floating(arg_index + 1, lit, v, false);
                return v;
            };
            auto read_i = [&](u32 arg_index) {
                i32 lit = 0;
                if (base + arg_index * 4 + 4 <= ctx.instruction->length)
                    std::memcpy(&lit, bytes + base + arg_index * 4, 4);
                i32 v = lit;
                ctx.integer(arg_index + 1, lit, v, false);
                return v;
            };
            EnemySpawnArgs args;
            float x = read_f(0), y = read_f(1);
            const bool relative = opcode == 300 || opcode == 309 || opcode == 321 || opcode == 311 || opcode == 304;
            if (relative) {
                args.pos.x = pos.x + x;
                args.pos.y = pos.y + y;
            } else {
                args.pos.x = x;
                args.pos.y = y;
            }
            if (opcode == 311 || opcode == 304 || opcode == 312 || opcode == 305) args.mirror = 1;
            if ((flags & kFlagMirror) != 0) {
                args.pos.x = args.pos.x * -1.0f;
                args.mirror ^= 1u;
            }
            args.life = float(read_i(2));
            args.score = read_i(3);
            args.item = read_i(4);
            if (!manager->spawn_enemy(sub, args)) { error = manager->error; return -2; }
            return 0;
        }
        case 302: anm_slot = iarg(0); return 0;
        case 500: {  // case 0x2d：首次设定判定时登记为“可计奖励”敌机
            hitbox.x = farg(0); hitbox.y = farg(1);
            if (score_bonus == 0) {
                score_bonus = 1;
                if (globals != nullptr) globals->enemy_bonus_pending += 1;
            }
            return 0;
        }
        case 501: hurtbox.x = farg(0); hurtbox.y = farg(1); return 0;
        case 502: flags |= u32(iarg(0)); return 0;
        case 503: flags &= ~u32(iarg(0)); return 0;
        case 504: {
            const float x = farg(0), y = farg(1), w = farg(2), h = farg(3);
            flags |= 0x20000u;
            bound_center = {x, y};
            bound_size = {w, h};
            return 0;
        }
        case 505: flags &= ~0x20000u; return 0;
        case 506: {  // case 0x33：清掉落表（保留季节道具数）
            std::memset(drop_counts, 0, sizeof drop_counts);
            return 0;
        }
        case 507: {  // case 0x34：掉落表 +0x3f90[arg0] = arg1（0 = 单掉道具种类、1..15 = 各类数量、16 = 季节道具数）
            const i32 slot = iarg(0), count = iarg(1);
            if (slot == 0) drop_item_type = count;
            else if (slot >= 1 && slot <= 15) drop_counts[slot - 1] = count;
            else if (slot == 16) season_drop_max = count;
            return 0;
        }
        case 509: {  // case 0x36：符卡练习模式外立即掉落
            if (globals != nullptr && (globals->flags & GameGlobals::kModeMask) == 0x20u) return 0;
            settle_season_drop();
            release_items();
            return 0;
        }
        case 508: {  // case 0x35
            const float rx = farg(0), ry = farg(1);
            drop_radius = {rx, ry};
            return 0;
        }
        case 510: drop_item_type = iarg(0); return 0;  // case 0x37
        case 556: {  // case 0x61：死亡子程序名
            if (!ctx.instruction) return 0;
            const auto* bytes = reinterpret_cast<const u8*>(ctx.instruction);
            const u32 limit = ctx.instruction->length;
            std::string name;
            for (u32 k = 20; k < limit && bytes[k] != 0; ++k) name.push_back(char(bytes[k]));
            death_sub = std::move(name);
            return 0;
        }
        case 562: release_items(); return 0;  // case 0x67
        case 511: {  // case 0x38
            life = life_max = life_display = iarg(0);
            if ((flags & 0x800000u) != 0) flags |= kFlagBossLife;
            life_budget = life * 7;
            return 0;
        }
        case 514: {  // case 0x3b：阶段中断登记（符卡练习里 Boss 槽 0 的中断一律改为 BossDead / BossEscape）
            if (!ctx.instruction) return 0;
            const auto* bytes = reinterpret_cast<const u8*>(ctx.instruction);
            const i32 slot = iarg(0), life_threshold = iarg(1), time_threshold = iarg(2);
            if (slot < 0 || slot >= kInterruptSlots) return 0;
            if (globals != nullptr && (globals->flags & GameGlobals::kModeMask) == 0x20u && (flags & 0x800000u) != 0) {
                const bool stage7_early = globals->stage == 7 && globals->spell_id < 0x29;
                const char* dead = boss_slot == 0 ? (stage7_early ? "MBossDead" : "BossDead")
                                                  : (stage7_early ? "MBossBDead" : "BossDead");
                set_interrupt(slot, 0, time_threshold, dead);
                interrupts[slot].timeout_sub = "BossEscape";
                return 0;
            }
            interrupts[slot].life_threshold = life_threshold;
            if (life_threshold >= 0) {
                std::string name;
                for (u32 k = 32; k < ctx.instruction->length && bytes[k] != 0; ++k) name.push_back(char(bytes[k]));
                set_interrupt(slot, life_threshold, time_threshold, name.c_str());
            }
            return 0;
        }
        case 521: {  // case 0x42：单独改超时子程序
            if (!ctx.instruction) return 0;
            const auto* bytes = reinterpret_cast<const u8*>(ctx.instruction);
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kInterruptSlots) return 0;
            std::string name;
            for (u32 k = 24; k < ctx.instruction->length && bytes[k] != 0; ++k) name.push_back(char(bytes[k]));
            interrupts[slot].timeout_sub = std::move(name);
            return 0;
        }
        case 516: if (manager) manager->sfx_queue.push_back(iarg(0)); return 0;  // case 0x3d
        case 517:  // case 0x3e：0x45d150(1, …) 画面震动（ScreenEffect，仅视觉且用 game_rng）——渲染侧接入时补
            (void)iarg(0); (void)iarg(1); (void)iarg(2);
            return 0;
        case 524: {  // case 0x45：阶段编号（DAT_004a5798 / Gui+0xa8），奖励归属重置
            const i32 v = iarg(0);
            if (globals) globals->spell_id = v;
            score_bonus_spell = v;
            score_bonus = 0;
            return 0;
        }
        case 525: if (manager) manager->kill_all(0, nullptr); return 0;     // case 0x46：0x41d900
        case 551: {  // case 0x5c：0x41da30（仅 anm_layer == arg0 的敌机）
            const i32 layer = iarg(0);
            if (manager) manager->kill_all(1, &layer);
            return 0;
        }
        case 571: if (manager) manager->kill_all(2, nullptr); return 0;     // case 0x70：0x41db70（并清死亡子程序）
        case 527: {  // case 0x48：Gui+0x1ec 阶段血条段（Gui 接入前只消费参数）
            (void)iarg(0); (void)farg(1); (void)iarg(2);
            return 0;
        }
        case 541: no_hit_timer.set(iarg(0)); return 0;  // case 0x52
        case 542: if (manager && manager->spellcard) manager->spellcard->flags |= Spellcard::kSurvival; return 0;
        case 543: {  // case 0x54
            if (manager && manager->spellcard) {
                manager->spellcard->flags |= Spellcard::kNoTimeout;
                if (manager->animations) manager->animations->kill(manager->spellcard->vm_ids[4]);
                manager->spellcard->vm_ids[4] = 0;
            }
            return 0;
        }
        case 546: {  // case 0x57：Bomb / 季节解放期间换用的主 VM 脚本
            flags = (flags & ~0x10000000u) | ((u32(iarg(0)) << 28) & 0x10000000u);
            flags &= ~0x20000001u;
            bomb_script = iarg(1);
            bomb_script_restore = main_script;
            return 0;
        }
        case 549: flags = (flags & 0x7fffffffu) | (u32(iarg(0)) << 31); return 0;  // case 0x5a
        case 565: damage_rate = farg(0); return 0;  // case 0x6a
        case 569: {  // case 0x6e：击破奖励登记（score_bonus 为 0 时登记，为 1（op 500 占位）时覆盖）
            if (score_bonus == 0) {
                const i32 v = iarg(0);
                score_bonus = v;
                if (v > 0 && globals) globals->enemy_bonus_pending += v;
            } else if (score_bonus == 1) {
                const i32 v = iarg(0);
                score_bonus = v;
                if (globals) globals->enemy_bonus_pending += v - 1;
            }
            return 0;
        }
        case 570: {  // case 0x6f：奖励结算到 enemy_bonus
            if (score_bonus != 0 && globals && score_bonus_spell == globals->spell_id) {
                globals->enemy_bonus += score_bonus;
                score_bonus = 0;
            }
            return 0;
        }
        case 572: {  // case 0x71
            life = iarg(0);
            life_budget = life * 7;
            return 0;
        }
        case 512: {
            const i32 slot = iarg(0);
            if (slot < 0) {
                if ((flags & 0x800000u) != 0 && manager && boss_slot >= 0 && boss_slot < 3)
                    manager->boss_ids[boss_slot] = 0;
                flags &= ~0x800000u;
                boss_slot = -1;
            } else if (slot < 3) {
                flags |= 0x800000u;
                boss_slot = slot;
                if (manager) manager->boss_ids[slot] = id;
            }
            return 0;
        }
        case 513: life_timer.set(0); return 0;
        case 515: invuln.set(iarg(0)); return 0;
        case 520: {
            if (manager) {
                for (u32 bid : manager->boss_ids)
                    if (bid && manager->find_id(bid)) return -1;
            }
            return 0;
        }
        case 518: case 519:
            // case 0x3f / 0x40：对话开始（Gui 0x429ff0 + 0x416f40 全屏消弹 + 0x431af0 道具吸引）与等待对话结束——Gui 接入时补
            return 0;
        case 400: {
            const float x = farg(0), y = farg(1);
            if (x > kSentinel) { motion1.pos.x = x; pos.x = motion1.pos.x + motion2.pos.x; }
            if (y > kSentinel) { motion1.pos.y = y; pos.y = motion1.pos.y + motion2.pos.y; }
            motion1.mode &= ~0xfu;
            return 0;
        }
        case 401: {
            const i32 frames = iarg(0), mode = iarg(1);
            const float x = farg(2), y = farg(3);
            if (frames <= 0) { pos_interp.end_time = 0; return 0; }
            pos_interp.initial = motion1.pos;
            pos_interp.goal = {x > kSentinel ? x : motion1.pos.x, y > kSentinel ? y : motion1.pos.y, 0.0f};
            pos_interp.bezier1 = {};
            pos_interp.bezier2 = {};
            pos_interp.mode = mode;
            pos_interp.end_time = frames;
            pos_interp.timer.set(0);
            motion1.mode &= ~0xfu;
            return 0;
        }
        case 404: case 406: {
            float angle = farg(0), speed = farg(1);
            if (angle > kSentinel) {
                if ((flags & kFlagMirror) != 0) angle = wrap_angle(-(angle - kPi) + kPi);
                motion1.angle = wrap_angle(angle);
            }
            if (speed > kSentinel) motion1.speed = speed;
            motion1.mode &= ~0xfu;
            return 0;
        }
        case 405: case 407: {
            const i32 frames = iarg(0);
            const i32 mode = iarg(1);
            float angle = farg(2), speed = farg(3);
            if (frames <= 0) { angle_interp.end_time = 0; speed_interp.end_time = 0; return 0; }
            if (angle > kSentinel) {
                if ((flags & kFlagMirror) != 0)
                    angle = wrap_angle(float(1.5707963705062866f - wrap_angle(float(angle - 1.5707963705062866f))));
                float start_a = motion1.angle;
                if (!(std::fabs(float(start_a - angle)) < kPi)) {
                    if (angle > start_a) start_a = float(start_a + 6.2831854820251465f);
                    else angle = float(angle + 6.2831854820251465f);
                }
                angle_interp.initial = start_a;
                angle_interp.goal = angle;
                angle_interp.mode = mode;
                angle_interp.end_time = frames;
                angle_interp.timer.set(0);
            }
            if (speed > kSentinel) {
                speed_interp.initial = motion1.speed;
                speed_interp.goal = speed;
                speed_interp.mode = mode;
                speed_interp.end_time = frames;
                speed_interp.timer.set(0);
            }
            motion1.mode &= ~0xfu;
            return 0;
        }
        case 412: {
            if (!rng) return fail("412: no rng");
            float angle = 0;
            const float x = pos.x, qw = bound_size.x * 0.25f;
            if (bound_center.x - qw > x) angle = float(float(rng->signed_unit() * kPi) / 3.0f);
            else if (x > bound_center.x + qw) angle = wrap_angle(float(float(float(rng->signed_unit() * kPi) / 3.0f) + kPi));
            else {
                angle = float(float(rng->signed_unit() * kPi) * 0.25f);
                const u16 w = rng->next16();
                const float px = player ? player->pos.x : 0.0f;
                if (px > x ? (w % 3 == 0) : (w % 3 != 0)) angle = float(angle + kPi);
            }
            const float qh = bound_size.y * 0.25f, y = pos.y;
            if (bound_center.y - qh > y) angle = std::fabs(angle);
            else if (y > bound_center.y + qh) angle = -std::fabs(angle);
            if (std::fabs(float(angle + kHalfPi)) < 0.05000000074505806f)
                angle = angle < -kHalfPi ? -1.6207963228225708f : -1.5207964181900024f;
            else if (double(std::fabs(float(angle - kHalfPi))) < 0.05)
                angle = angle < kHalfPi ? 1.5207964181900024f : 1.6207963228225708f;
            const i32 duration = iarg(0), mode = iarg(1);
            const float speed = farg(2);
            angle_interp.initial = angle;
            angle_interp.goal = angle;
            angle_interp.mode = mode;
            angle_interp.end_time = duration;
            angle_interp.timer.set(0);
            speed_interp.initial = speed;
            speed_interp.goal = 0.0f;
            speed_interp.end_time = duration;
            speed_interp.timer.set(0);
            motion1.mode &= ~0xfu;
            return 0;
        }
        case 1000: {
            const i32 d = iarg(0);
            season_drop_duration = d;
            season_drop_timer.set(d);
            season_drop_max = iarg(1);
            season_drop_min = iarg(2);
            return 0;
        }
        case 1001:  // case 0xac：按累计伤害放季节道具
            season_item_step = iarg(0);
            season_item_next = damage_accum;
            return 0;
        // —— ANM 绑定 302..340（0x41dcb0 case 1..0x15 / 0x423260 / 0x4233a0）——
        case 303: {  // 0x423260
            const i32 slot = iarg(0);
            const i32 script = iarg(1);
            if (slot < 0 || slot >= kVmSlots) return 0;
            if (manager && manager->animations) manager->animations->kill(vm_ids[slot]);
            vm_ids[slot] = 0;
            if (script < 0) return 0;
            if (slot == 0) { main_script = script; anm_slot_main = anm_slot; }
            rebind_vm(slot, anm_slot, script, true);
            return 0;
        }
        case 306: {  // case 3：主体精灵 + 朝向脚本族
            const i32 slot = iarg(0);
            const i32 script = iarg(1);
            if (slot < 0 || slot >= kVmSlots) return 0;
            rebind_vm(slot, anm_slot, script, true);
            if (slot == 0) {
                flags |= 0x100000u;
                main_script_base = main_script = script;
                sprite_direction = 0;
                anm_slot_main = anm_slot;
            }
            return 0;
        }
        case 307: {  // case 4：以敌机坐标为 entity_offset 的独立 VM（链表头，creation_flags 2）
            const i32 anm = iarg(0), script = iarg(1);
            AnmFile* file = anm_file(anm);
            if (file == nullptr || !manager->animations) return 0;
            const u32 vid = manager->animations->spawn_vm(*file, script, -1, pos, 0.0f, 2, true);
            if (effects) effects->register_id(vid);
            return 0;
        }
        case 308: {  // case 5
            const i32 anm = iarg(0), script = iarg(1);
            AnmFile* file = anm_file(anm);
            if (file == nullptr || !manager->animations) return 0;
            const u32 vid = manager->animations->spawn_front(*file, script, -1);
            if (effects) effects->register_id(vid);
            return 0;
        }
        case 313: {  // case 7：朝向族 +5 脚本
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kVmSlots) return 0;
            rebind_vm(slot, anm_slot, main_script_base + 5, true);
            return 0;
        }
        case 314: {  // case 8：0x40e5c0 特效 VM（entity_offset = pos）
            const i32 script = iarg(1), anm = iarg(0);
            AnmFile* file = anm_file(anm);
            if (file == nullptr || !manager->animations) return 0;
            const u32 vid = manager->animations->spawn_effect(*file, script, -1, pos, 0.0f);
            if (effects) effects->register_id(vid);
            return 0;
        }
        case 315: {  // case 9：同 307 但带 rotation.z
            const i32 anm = iarg(0), script = iarg(1);
            const float rot = farg(2);
            AnmFile* file = anm_file(anm);
            if (file == nullptr || !manager->animations) return 0;
            const u32 vid = manager->animations->spawn_vm(*file, script, -1, pos, 0.0f, 2, true);
            if (AnmVm* vm = manager->animations->find(vid)) {
                vm->entity_offset = pos;
                vm->flags_lo |= AnmVm::kRotationDirty;
                vm->rotation.z = rot;
            }
            if (effects) effects->register_id(vid);
            return 0;
        }
        case 316: {  // case 0xa：朝向族 +5+n（n < 0 → 基号）
            const i32 slot = iarg(0), n = iarg(1);
            if (slot < 0 || slot >= kVmSlots) return 0;
            rebind_vm(slot, anm_slot, n < 0 ? main_script_base : main_script_base + 5 + n, true);
            return 0;
        }
        case 317: {  // case 0xb：0x46f0b0 中断
            const i32 slot = iarg(0), value = iarg(1);
            if (slot < 0 || slot >= kVmSlots || !manager->animations) return 0;
            manager->animations->interrupt(vm_ids[slot], value);
            return 0;
        }
        case 318: {  // case 0xc：回到基号脚本并关闭朝向切换
            rebind_vm(0, anm_slot, main_script_base, false);
            flags &= ~0x100000u;
            main_script = main_script_base;
            sprite_direction = 0;
            anm_slot_main = anm_slot;
            return 0;
        }
        case 319: case 325: case 326: case 327: case 328: case 329: case 330: case 331: case 332: case 333:
        case 335: case 336: case 337: {  // 0x4233a0
            const i32 slot = iarg(0);
            if (u32(slot) >= 0x10u) return 0;
            AnmVm* vm = vm_at(slot);
            if (vm == nullptr) return 0;
            switch (opcode) {
                case 319: vm->flags_lo |= AnmVm::kRotationDirty; vm->rotation.z = farg(1); break;
                case 325: {
                    const i32 b = iarg(3), g = iarg(2), r = iarg(1);
                    vm->color[2] = u8(r); vm->color[1] = u8(g); vm->color[0] = u8(b);
                    break;
                }
                case 326: {
                    const u8 r = u8(iarg(3)), g = u8(iarg(4)), b = u8(iarg(5));
                    const i32 mode = iarg(2), time = iarg(1);
                    anm_begin_color_interp(vm->color_interp, time, mode, vm->color, b, g, r);
                    break;
                }
                case 327: vm->color[3] = u8(iarg(1)); break;
                case 328: {
                    const i32 goal = iarg(3), mode = iarg(2), time = iarg(1);
                    anm_begin_scalar_interp(vm->alpha_interp, time, mode, vm->color[3], goal & 0xff);
                    break;
                }
                case 329: {
                    const float y = farg(2), x = farg(1);
                    vm->flags_lo |= AnmVm::kScaleDirty;
                    vm->scale = {x, y};
                    break;
                }
                case 330: {
                    const float y = farg(4), x = farg(3);
                    const i32 mode = iarg(2), time = iarg(1);
                    anm_begin_vec2_interp(vm->scale_interp, time, mode, vm->scale, {x, y});
                    break;
                }
                case 331: vm->color2[3] = u8(iarg(1)); break;
                case 332: {
                    const i32 goal = iarg(3), mode = iarg(2), time = iarg(1);
                    anm_begin_scalar_interp(vm->alpha2_interp, time, mode, vm->color2[3], goal & 0xff);
                    vm->flags_lo = (vm->flags_lo & ~0x40000u) | 0x20000u;
                    break;
                }
                case 333: {
                    const float y = farg(4), x = farg(3);
                    (void)iarg(0);
                    const i32 mode = iarg(2), time = iarg(1);
                    anm_begin_vec3_interp(vm->pos_interp, time, mode, vm->entity_offset, {x, y, 0.0f});
                    break;
                }
                case 335: {
                    const float y = farg(2), x = farg(1);
                    vm->flags_lo |= AnmVm::kScaleDirty;
                    vm->scale2 = {x, y};
                    break;
                }
                case 336: anm_set_layer(*vm, iarg(1)); break;
                case 337: {
                    const u32 blend = u32(iarg(1)) & 0xffu;
                    vm->flags_lo ^= ((blend << 5) ^ vm->flags_lo) & 0x1e0u;
                    break;
                }
                default: break;
            }
            return 0;
        }
        case 320: {  // case 0xe
            const i32 slot = iarg(0);
            const float x = farg(1), y = farg(2);
            if (slot < 0 || slot >= kVmSyncSlots) return 0;
            vm_offsets[slot] = {x, y, 0.0f};
            return 0;
        }
        case 322: {  // case 0xf
            const i32 parent = iarg(1), slot = iarg(0);
            if (slot < 0 || slot >= kVmSyncSlots) return 0;
            vm_parent_slot[slot] = parent;
            return 0;
        }
        case 323: death_anm_slot = iarg(0); death_script = iarg(1); return 0;  // case 0x10
        case 324: {  // case 0x11：取指定敌机坐标（找不到 → 自身）
            const Enemy* target = manager ? manager->find_id(u32(iarg(2))) : nullptr;
            float* dx = ctx.float_destination(0);
            if (!dx) return -2;
            *dx = target ? target->pos.x : pos.x;
            float* dy = ctx.float_destination(1);
            if (!dy) return -2;
            *dy = target ? target->pos.y : pos.y;
            return 0;
        }
        case 334: if (effects) effects->spawn(iarg(0), pos); return 0;  // case 0x12：0x40e730
        case 338: {  // case 0x13：偏移 + rotation.z 的独立 VM
            const float dy = farg(3), dx = farg(2);
            const Vec3 offset{dx + pos.x, pos.y + dy, pos.z + 0.0f};
            const float rot = farg(4);
            const i32 script = iarg(1), anm = iarg(0);
            AnmFile* file = anm_file(anm);
            if (file == nullptr || !manager->animations) return 0;
            const u32 vid = manager->animations->spawn_vm(*file, script, -1, offset, rot, 2, true);
            if (effects) effects->register_id(vid);
            return 0;
        }
        case 339: {  // case 0x14：生成后再预跑 arg2 帧
            const i32 anm = iarg(0), script = iarg(1);
            AnmFile* file = anm_file(anm);
            if (file == nullptr || !manager->animations) return 0;
            const u32 vid = manager->animations->spawn_front(*file, script, -1);
            if (effects) effects->register_id(vid);
            AnmVm* vm = manager->animations->find(vid);
            for (i32 i = 0; i < iarg(2); ++i) {
                if (vm) manager->animations->run(*vm);
            }
            return 0;
        }
        case 340: {  // case 0x15
            if (Enemy* target = manager ? manager->find_id(u32(iarg(0))) : nullptr) target->flags |= 0x2000000u;
            return 0;
        }
        // —— 弹幕发射器 600..641（0x42085b..0x4226d0）——
        case 600: {  // 0x42085b
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            shooters[slot].reset();
            shooter_offset[slot].x = 0;
            shooter_offset[slot].y = 0;
            shooter_pos2[slot] = Vec3{};
            shooter_ex_counter[slot] = 0;
            return 0;
        }
        case 601: {  // 0x4209cf
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots || !bullets) return 0;
            EtShooter& s = shooters[slot];
            const Vec3& off = shooter_offset[slot];
            if (shooter_pos2[slot].z > 0.9f) {
                s.pos = {shooter_pos2[slot].x + off.x, off.y + shooter_pos2[slot].y, 0.0f};
            } else {
                s.pos = {off.x + pos.x, off.y + pos.y, off.z + pos.z};
            }
            bullets->safe_radius_sq = safe_radius_sq;
            bullets->shoot(s);
            bullets->safe_radius_sq = 0;
            return 0;
        }
        case 602: {  // 0x420ab6
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            shooters[slot].type = i16(iarg(1));
            shooters[slot].color = i16(iarg(2));
            return 0;
        }
        case 603: {  // 0x420af0
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            shooter_offset[slot].x = farg(1);
            shooter_offset[slot].y = farg(2);
            return 0;
        }
        case 604: {  // 0x420c34
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            shooters[slot].angle1 = farg(1);
            shooters[slot].angle2 = farg(2);
            return 0;
        }
        case 605: {  // 0x420c72
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            shooters[slot].speed1 = farg(1);
            shooters[slot].speed2 = farg(2);
            return 0;
        }
        case 606: {  // 0x420cb0
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            shooters[slot].way = i16(iarg(1));
            shooters[slot].stack = i16(iarg(2));
            return 0;
        }
        case 607: {  // 0x421152
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            shooters[slot].aim_mode = i16(iarg(1));
            return 0;
        }
        case 608: {  // 0x42117f
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            shooters[slot].shoot_sfx = iarg(1);
            shooters[slot].step_sfx = iarg(2);
            return 0;
        }
        case 609: case 610: case 611: case 612: {  // 0x4211b9：写 Ex 步骤（609/610 指定下标，611/612 用递增计数）
            i32 slot = iarg(0);
            if (slot >= 18 || slot < 0) slot = 0;
            if (slot >= kShooterSlots) return 0;
            i32 idx;
            u32 next;
            if (opcode == 609 || opcode == 610) { idx = iarg(1); next = 2; }
            else { idx = shooter_ex_counter[slot]; next = 1; }
            if (idx < 0 || idx >= 18) return 0;
            BulletExStep& step = shooters[slot].ex[idx];
            const bool wide = (opcode == 610 || opcode == 612);
            step.async = iarg(next);
            step.type = u32(iarg(next + 1));
            step.a = iarg(next + 2);
            step.b = iarg(next + 3);
            next += 4;
            if (wide) { step.c = iarg(next); step.d = iarg(next + 1); next += 2; }
            step.r = farg(next);
            step.s = farg(next + 1);
            next += 2;
            if (wide) { step.m = farg(next); step.n = farg(next + 1); }
            if (opcode == 609 || opcode == 610) shooter_ex_counter[slot] = idx + 1;
            else shooter_ex_counter[slot] = shooter_ex_counter[slot] + 1;
            return 0;
        }
        case 613:  // 0x421655：全屏消弹（0x416f40 的 fastcall 参数为残留的操作码 613，仅计数）+ 0x431af0(1) 道具吸引
            if (bullets) bullets->cancel_all(613);
            // 0x431af0(1)：ItemManager 接入时补
            return 0;
        case 614: {  // 0x420925：复制发射器（含偏移与绝对坐标，不含 Ex 计数）
            const i32 dst = iarg(0), src = iarg(1);
            if (dst < 0 || dst >= kShooterSlots || src < 0 || src >= kShooterSlots) return 0;
            shooters[dst] = shooters[src];
            shooter_offset[dst] = shooter_offset[src];
            shooter_pos2[dst] = shooter_pos2[src];
            return 0;
        }
        case 615: case 616: case 635: case 636: {  // 0x42219e / 0x42221d / 0x422253 / 0x422289：圆形消弹 + 道具吸引
            const float radius = farg(0);
            const bool items = (opcode == 615 || opcode == 635);
            if (bullets) bullets->cancel_radius(pos, radius, items ? 1 : 0, opcode >= 635);
            // 0x431a70(pos, items, 1)：ItemManager 接入时补
            return 0;
        }
        case 617: {  // 0x420ded：按秩三档选速度
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            const i32 rank = globals ? globals->rank : 0;
            u32 base = 3;
            if (rank >= 0x200) base = 5;
            else if (rank < -0x200) base = 1;
            shooters[slot].speed1 = farg(base);
            shooters[slot].speed2 = farg(base + 1);
            return 0;
        }
        case 618: {  // 0x420e51：按秩五档选速度
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            const i32 rank = globals ? globals->rank : 0;
            u32 base = 1;
            if (rank >= 600) base = 9;
            else if (rank >= 200) base = 7;
            else if (rank >= -200) base = 5;
            else if (rank >= -600) base = 3;
            shooters[slot].speed1 = farg(base);
            shooters[slot].speed2 = farg(base + 1);
            return 0;
        }
        case 619: {  // 0x420ed4：按秩线性插值速度（rank+1024 / 2048）
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            const float f1 = farg(1), f2 = farg(2), f3 = farg(3), f4 = farg(4);
            const float t = float(globals ? globals->rank : 0) + 1024.0f;
            shooters[slot].speed1 = ((f3 - f1) * t) * 0.00048828125f + f1;
            shooters[slot].speed2 = ((f4 - f2) * t) * 0.00048828125f + f2;
            return 0;
        }
        case 620: {  // 0x420fa7：按秩三档选 way/stack
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            const i32 rank = globals ? globals->rank : 0;
            u32 base = 3;
            if (rank >= 0x200) base = 5;
            else if (rank < -0x200) base = 1;
            shooters[slot].way = i16(iarg(base));
            shooters[slot].stack = i16(iarg(base + 1));
            return 0;
        }
        case 621: {  // 0x421002：按秩五档选 way/stack
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            const i32 rank = globals ? globals->rank : 0;
            u32 base = 1;
            if (rank >= 600) base = 9;
            else if (rank >= 200) base = 7;
            else if (rank >= -200) base = 5;
            else if (rank >= -600) base = 3;
            shooters[slot].way = i16(iarg(base));
            shooters[slot].stack = i16(iarg(base + 1));
            return 0;
        }
        case 622: {  // 0x42107c：按秩线性插值 way/stack（有符号 /2048 向零截断）
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            const i32 a1 = iarg(1), a2 = iarg(2), a3 = iarg(3), a4 = iarg(4);
            const i32 t = (globals ? globals->rank : 0) + 0x400;
            auto lerp = [t](i32 lo, i32 hi) -> i16 {
                const i32 p = t * (hi - lo);
                return i16(((p + ((p >> 31) & 0x7ff)) >> 11) + lo);
            };
            shooters[slot].way = lerp(a1, a3);
            shooters[slot].stack = lerp(a2, a4);
            return 0;
        }
        case 623: {  // 0x4225d1：(x,y) → 自机方向角
            const float px = player ? player->pos.x : 0.0f;
            const float py = player ? player->pos.y : 0.0f;
            const float dx = px - farg(1);
            const float dy = py - farg(2);
            const float a = float(std::atan2(double(dy), double(dx)));
            float* dst = ctx.float_destination(0);
            if (!dst) return -2;
            *dst = a;
            return 0;
        }
        case 624: {  // 0x420cec：按难度选速度
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            const i32 d = globals ? globals->difficulty : 0;
            const u32 base = (d == 0) ? 1 : (d == 1) ? 2 : (d == 2) ? 3 : 4;
            shooters[slot].speed1 = farg(base);
            shooters[slot].speed2 = farg(base + 4);
            return 0;
        }
        case 625: {  // 0x420d64：按难度选 way/stack
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            const i32 d = globals ? globals->difficulty : 0;
            const u32 base = (d == 0) ? 1 : (d == 1) ? 2 : (d == 2) ? 3 : 4;
            shooters[slot].way = i16(iarg(base));
            shooters[slot].stack = i16(iarg(base + 4));
            return 0;
        }
        case 626: {  // 0x420b25：极坐标偏移
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            const float a = farg(1), len = farg(2);
            shooter_offset[slot].x = float(std::cos(double(a)) * double(len));
            shooter_offset[slot].y = float(std::sin(double(a)) * double(len));
            return 0;
        }
        case 627: {  // 0x420b95
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            shooters[slot].distance = farg(1);
            return 0;
        }
        case 628: {  // 0x420bc3：绝对发射坐标（x < -990 关闭）
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            const float x = farg(1), y = farg(2);
            shooter_pos2[slot] = {x, y, (x < -990.0f) ? 0.0f : 1.0f};
            return 0;
        }
        case 640: {  // 0x4213e9：Ex 步骤的子程序名（Ex 0x1000000 用；字符串位于指令 +0x1c）
            const i32 slot = iarg(0), idx = iarg(1);
            if (slot < 0 || slot >= kShooterSlots || idx < 0 || idx >= 18 || !ctx.instruction) return 0;
            shooters[slot].ex[idx].str = reinterpret_cast<const char*>(ctx.instruction) + 0x1c;
            return 0;
        }
        case 641: {  // 0x4213c2：Ex 计数 -1（下限 0）
            const i32 slot = iarg(0);
            if (slot < 0 || slot >= kShooterSlots) return 0;
            if (--shooter_ex_counter[slot] < 0) shooter_ex_counter[slot] = 0;
            return 0;
        }
        case 526: {  // 0x41ff5d
            const float r = farg(0);
            safe_radius_sq = r * r;
            return 0;
        }
        case 629: case 630: case 631: case 632: case 633: case 634: case 637: case 638: case 639:
            // 629 消弹光环特效（0x418c70）、630 STD 标签跳转、631 EnemyManager 标志、632–634/637/639 敌机回调表、
            // 638 得分飘字：分别随 Effect / Stage / Gui 子系统接入
            return 0;
        case 535: {
            i32 vals[4]{iarg(1), iarg(2), iarg(3), iarg(4)};
            i32* dst = ctx.integer_destination(0);
            if (!dst) return -2;
            const i32 d = (globals && globals->difficulty >= 0 && globals->difficulty < 3) ? globals->difficulty : 3;
            *dst = vals[d];
            return 0;
        }
        case 536: {
            float vals[4]{farg(1), farg(2), farg(3), farg(4)};
            float* dst = ctx.float_destination(0);
            if (!dst) return -2;
            const i32 d = (globals && globals->difficulty >= 0 && globals->difficulty < 3) ? globals->difficulty : 3;
            *dst = vals[d];
            return 0;
        }
        case 548: {
            i32 delays[4]{iarg(0), iarg(1), iarg(2), iarg(3)};
            const i32 d = (globals && globals->difficulty >= 0 && globals->difficulty < 3) ? globals->difficulty : 3;
            ctx.time = float(ctx.time - float(delays[d]));
            return 0;
        }
        case 522: case 528: case 537: case 538: case 539: {
            // case 0x43：0x417f00 开始符卡（Spellcard 接入时补 ANM / Gui 部分），置 7 倍减伤位并重算血量池，
            // life_timer 归零（switchD_0041dd51_caseD_3a）
            life_flags |= 1u;
            life_budget = life * 7;
            life_timer.set(0);
            return 0;
        }
        case 523:  // case 0x44：0x4182f0 结束符卡（Spellcard 接入时补），清 7 倍减伤位
            life_flags &= ~1u;
            return 0;
        case 568:
            life_flags = (life_flags & ~1u) | (u32(iarg(0)) & 1u);
            return 0;
        case 550: anm_layer = iarg(0); return 0;         // case 0x5b
        case 552: anm_layer_offset = iarg(0); return 0;  // case 0x5d
        case 561: {  // case 0x66：死亡音效 + 死亡特效（朝向 = 最后受击点 → 敌机）
            if (death_sfx >= 0 && manager) manager->sfx_queue.push_back(death_sfx);
            if (death_script < 0) return 0;
            const float ddx = hit_pos.x - pos.x, ddy = hit_pos.y - pos.y;
            float angle = -1.5707964f;
            if (!(0.04f > ddx * ddx + ddy * ddy)) angle = atan2_f(pos.y - hit_pos.y, pos.x - hit_pos.x);
            AnmFile* file = anm_file(death_anm_slot);
            if (file && manager->animations) manager->animations->spawn_effect(*file, death_script, 3, pos, angle);
            return 0;
        }
        case 545:
            return 0;
        default:
            // 未识别操作码：先空操作以免整条 ECL 链因后续关卡指令中断；oracle 对拍时再逐个补全
            return 0;
    }
}

bool EnemyManager::create(const char* ecl_name, GameGlobals& g, Player* pl, Rng& r,
                          AnmManager& anm, u8 difficulty) {
    globals = &g;
    player = pl;
    rng = &r;
    animations = &anm;
    enemies.clear();
    next_id = 1;
    last_id = 0;
    enemy_count = 0;
    timer.set(0);
    program = EclProgram{};
    if (!program.load(ecl_name, *this)) { error = program.error; return false; }
    (void)difficulty;
    return true;
}

bool EnemyManager::spawn_main() {
    EnemySpawnArgs args;
    args.pos = {};
    args.life = 0;
    return spawn_enemy("main", args) != nullptr;
}

Enemy* EnemyManager::spawn_enemy(const char* sub, const EnemySpawnArgs& args) {
    auto e = std::make_unique<Enemy>();
    const u8 diff = globals ? u8(1u << (globals->difficulty & 31)) : 0xff;
    if (!e->init(*this, program, sub, args, *globals, player, *rng, diff)) {
        error = e->error;
        return nullptr;
    }
    Enemy* raw = e.get();
    enemies.push_back(std::move(e));
    ++enemy_count;
    return raw;
}

Enemy* EnemyManager::find_id(u32 id) noexcept {
    for (auto& e : enemies) if (e && e->id == id) return e.get();
    return nullptr;
}

void EnemyManager::update(float game_speed) {
    // 0x41b3d0：先保存 next，生成中 push_back 的新敌人本帧不再走完整更新；kill_all 标过 0x2000000 的直接移出
    damage_dealt_bomb = 0;
    damage_dealt = 0;
    for (auto it = enemies.begin(); it != enemies.end();) {
        auto next = std::next(it);
        auto& e = *it;
        if (!e) { it = enemies.erase(it); continue; }
        if ((e->flags & 0x2000000u) != 0 || !e->update(game_speed)) {
            e->destroy();
            it = enemies.erase(it);
            if (enemy_count > 0) --enemy_count;
            continue;
        }
        e->flags &= ~kFlagReentry;
        it = next;
    }
    // 自机伤害倍率（0x2c7cc）每帧复位为 1；> 1.01 时置 Player flags & 0x20
    if (player != nullptr) {
        if (player->scale2 <= 1.01f) player->flags &= ~0x20u;
        else player->flags |= 0x20u;
        player->scale2 = 1.0f;
    }
    timer.tick(game_speed);
}

// 0x41d900（mode 0）/ 0x41da30（mode 1：仅 anm_slot == *filter）/ 0x41db70（mode 2：并清死亡子程序）
void EnemyManager::kill_all(int mode, const i32* filter) {
    for (auto& e : enemies) {
        if (!e) continue;
        if ((e->flags & 0xc004a0u) != 0 && (e->flags & 0x100u) == 0) continue;
        if (mode == 1 && (filter == nullptr || e->anm_layer != *filter)) continue;
        e->drop_item_type = 0;
        std::memset(e->drop_counts, 0, sizeof e->drop_counts);
        e->season_drop_max = 0;
        e->drop_radius = {32.0f, 32.0f};
        e->hit_pos = {0.0f, 192.0f, e->hit_pos.z};
        e->score_bonus = 0;
        if (mode == 2) e->death_sub.clear();
        e->die(1.0f);
        e->flags |= 0x2000000u;
    }
    timer.tick(1.0f);
}

}  // namespace th16
