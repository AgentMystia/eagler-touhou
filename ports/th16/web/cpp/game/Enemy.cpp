// th16 Enemy / EnemyManager 还原——见 Enemy.hpp 锚点。
#include "Enemy.hpp"
#include "AnmManager.hpp"
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
}  // namespace

const char* Enemy::routine_name() const noexcept {
    if (!program || primary.routine < 0 || u32(primary.routine) >= program->definitions.size()) return nullptr;
    return program->subroutine(u32(primary.routine)).name.c_str();
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
        case -9969: value = truncate_int(motion1.angle); return true;
        case -9968: value = truncate_int(motion2.angle); return true;
        case -9967: value = truncate_int(motion1.speed); return true;
        case -9966: value = truncate_int(motion2.speed); return true;
        case -9960: value = 0; return true;  // DAT_004a57bc 秩/等级，后续接全局
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
        case -9969: value = motion1.angle; return true;
        case -9968: value = motion2.angle; return true;
        case -9967: value = motion1.speed; return true;
        case -9966: value = motion2.speed; return true;
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

bool Enemy::spawn(EclContext& source, i32 id, u32 /*skipped_arguments*/) {
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
    const u32 name_size = source.instruction->argument<u32>(0);
    const auto* bytes = reinterpret_cast<const u8*>(source.instruction);
    if (name_size == 0 || name_size > source.instruction->length - 20) return false;
    const i32 routine = program->find_index(reinterpret_cast<const char*>(bytes + 20));
    if (routine < 0) { error = "async sub missing"; return false; }
    if (!ctx->bind(*program, routine)) { error = ctx->error; return false; }
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
    id = mgr.next_id;
    mgr.next_id = mgr.next_id + 1;
    if (mgr.next_id == 0) mgr.next_id = 1;
    flags = 0;
    if (args.mirror) flags |= kFlagMirror;
    if (args.life >= 1000) flags |= kFlagBossLife;
    pos = args.pos;
    motion1 = MotionState{};
    motion2 = MotionState{};
    motion1.pos = args.pos;
    life = truncate_int(args.life);
    life_max = life;
    int_regs[0] = 1;  // 0x41aa70：puVar6[0x1498] = 1
    invuln.set(2);
    life_timer.set(0x3c);
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
    pos.x = floor_f((motion1.pos.x + motion2.pos.x) * 100.0f) / 100.0f;
    pos.y = floor_f((motion1.pos.y + motion2.pos.y) * 100.0f) / 100.0f;
    pos.z = motion1.pos.z + motion2.pos.z;
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

    // 0x41bb50 尾：按主 VM 精灵宽高（+0x3f58/+0x3f5c，op 306 前为 0）判定进入场地与出界销毁
    const float half_w = sprite_size.x * 0.5f, half_h = sprite_size.y * 0.5f;
    if (pos.x + half_w < -192.0f || 192.0f < pos.x - half_w) {
        if ((flags & 0x10004u) == 0x10000u) { alive = false; return false; }
    } else if (pos.y + half_h < 0.0f || 448.0f < pos.y - half_h) {
        if ((flags & kFlagAlive) != 0 && (flags & 8u) == 0) { alive = false; return false; }
    } else {
        flags |= kFlagAlive;
    }

    primary.interpolation_rate = game_speed;
    int r = primary.step(game_speed);
    if (r == -2) { error = primary.error; alive = false; return false; }
    if (r == -1) { alive = false; return false; }
    for (auto it = async_threads.begin(); it != async_threads.end();) {
        auto& t = *it;
        if (!t || t->instruction_offset < 0) { it = async_threads.erase(it); continue; }
        t->interpolation_rate = game_speed;
        const int ar = t->step(game_speed);
        if (ar == -2) { error = t->error; alive = false; return false; }
        if (ar == -1) { it = async_threads.erase(it); continue; }
        ++it;
    }

    if (invuln.current > 0) invuln.untick(game_speed);
    // 注意：不在这里清除 kFlagReentry——由 EnemyManager::update 清除（对齐 0x41b3d0）
    return true;
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
        case 300: case 301: case 304: case 305: {
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
            if (opcode == 300 || opcode == 304) {
                args.pos.x = pos.x + x;
                args.pos.y = pos.y + y;
            } else {
                args.pos.x = x;
                args.pos.y = y;
            }
            if (opcode == 304 || opcode == 305) args.mirror = 1;
            args.life = float(read_i(2));
            args.score = read_i(3);
            args.item = read_i(4);
            if (!manager->spawn_enemy(sub, args)) { error = manager->error; return -2; }
            return 0;
        }
        case 302: anm_slot = iarg(0); return 0;
        case 500: hitbox.x = farg(0); hitbox.y = farg(1); return 0;
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
        case 510: /* 受击/死亡特效颜色（+0x4054） */ return 0;
        case 511: {
            life = life_max = iarg(0);
            if ((flags & 0x800000u) != 0) flags |= kFlagBossLife;
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
        case 516: case 518: case 519: case 523: case 524: case 525:
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
            if (!(angle > kSentinel)) angle = motion1.angle;
            else if ((flags & kFlagMirror) != 0) angle = wrap_angle(float(1.5707963705062866f - wrap_angle(float(angle - 1.5707963705062866f))));
            if (!(speed > kSentinel)) speed = motion1.speed;
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
            speed_interp.initial = motion1.speed;
            speed_interp.goal = speed;
            speed_interp.mode = mode;
            speed_interp.end_time = frames;
            speed_interp.timer.set(0);
            motion1.mode &= ~0xfu;
            return 0;
        }
        case 412: {
            if (!rng) return fail("412: no rng");
            constexpr float kHalfPi = 1.5707963705062866f;
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
        case 1000:
            drop_timer = iarg(0);
            drop_interval = iarg(1);
            drop_type = iarg(2);
            return 0;
        case 1001: return 0;
        case 306: {
            // 0x41dcb0 case 3：slot == 0 时设精灵宽高（妖精 32×32）并置 flags |= 0x100000
            const i32 slot = iarg(0);
            if (slot == 0) {
                sprite_size = {32.0f, 32.0f};
                flags |= 0x100000u;
            }
            return 0;
        }
        case 303: {
            const i32 slot = iarg(0);
            if (slot == 0) sprite_size = {32.0f, 32.0f};
            return 0;
        }
        case 307: case 308: case 309: case 313: case 316: case 319:
        case 320: case 321: case 322: case 323: case 324: case 325:
            // ANM 绑定/偏移/层：渲染未接前空操作
            return 0;
        case 600: case 601: case 602: case 603: case 604: case 605: case 606: case 607:
        case 611: case 615: case 616: case 629: case 630: case 632:
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
        case 537: case 545: case 550: case 552:
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
    // 0x41b3d0：先保存 next，生成中 push_back 的新敌人本帧不再走完整更新
    for (auto it = enemies.begin(); it != enemies.end();) {
        auto next = std::next(it);
        auto& e = *it;
        if (!e) { it = enemies.erase(it); continue; }
        if (!e->update(game_speed)) {
            it = enemies.erase(it);
            if (enemy_count > 0) --enemy_count;
            continue;
        }
        e->flags &= ~kFlagReentry;
        it = next;
    }
    timer.tick(game_speed);
}

}  // namespace th16
