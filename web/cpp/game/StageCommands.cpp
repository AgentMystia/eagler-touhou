// stage1 游戏指令执行（0x41b260 处理块的行为子集）。锚点注释对应 ecl-vm.md §4.2。
// 未实现指令一律置 sim->error 并返回 -2（harness 非零退出）——不静默吞掉契约缺口。
#include "StageSim.hpp"

#include "StageMath.hpp"

#include <cmath>
#include <cstring>

namespace th13 {

namespace {
constexpr float kKeep = -999999.0f;

bool resolve_float(EclContext& c, u32 argument, float& value) {
    return c.floating(argument, c.instruction->argument<float>(argument), value);
}

bool resolve_int(EclContext& c, u32 argument, i32& value) {
    return c.integer(argument, c.instruction->argument<i32>(argument), value);
}

void shortest_arc(float& from, float& to) {
    const float diff = to >= from ? to - from : from - to;
    if (diff < 3.1415927f) return;
    if (to <= from)
        to = to + 6.2831855f;
    else
        from = from + 6.2831855f;
}

u32 postname_dword(const EclInstruction* ins, u32 index) {
    const u32 name_length = u32(ins->argument<i32>(0));
    const u32 base = ((name_length + 4) >> 2) + index;  // 0x41fcc0:29-30
    u32 raw = 0;
    std::memcpy(&raw, reinterpret_cast<const u8*>(ins) + 16 + base * 4, 4);
    return raw;
}

bool resolve_postname_float(EclContext& c, u32 index, u32 argument, float& value) {
    const u32 raw = postname_dword(c.instruction, index);
    float literal;
    std::memcpy(&literal, &raw, 4);
    return c.floating(argument, literal, value);
}

bool resolve_postname_int(EclContext& c, u32 index, u32 argument, i32& value) {
    const u32 raw = postname_dword(c.instruction, index);
    return c.integer(argument, i32(raw), value);
}

// 镜像翻转表（0x41c137/0x41c21f）：非 NM 变体在敌机镜像位下翻转角度。
bool mirrored_variant(u16 op) { return op == 404 || op == 405 || op == 406 || op == 407; }
}  // namespace

int StageCommands::execute(EclContext& c, u16 op) {
    StageSim& sim = *this->sim;
    StageEnemy& e = *sim.current;
    const EclInstruction* ins = c.instruction;

    // ---- 事件记录辅助：enemy_move（解析后实参，指令序）----
    const auto emit_move = [&](const u32* integral) {
        float values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        const u32 count = ins->parameter_count < 8 ? ins->parameter_count : 8;
        for (u32 i = 0; i < count; ++i) {
            i32 raw = 0;
            std::memcpy(&raw, reinterpret_cast<const u8*>(ins) + 16 + i * 4, 4);
            float literal;
            std::memcpy(&literal, &raw, 4);  // 同一 dword 的 float 位视图
            if (integral && integral[i]) {
                i32 v = 0;
                if (!c.integer(i, raw, v, false) && c.error.empty()) c.error = "move arg resolve";
                if (!c.error.empty()) {
                    sim.error_ = c.error;
                    return false;
                }
                values[i] = float(v);  // 整型参数按整数记录
            } else if (!c.floating(i, literal, values[i], false)) {
                sim.error_ = c.error.empty() ? "move arg resolve" : c.error;
                return false;
            }
        }
        sim.sink.enemy_move(e.serial, op, integral, values, count);
        return true;
    };

    switch (op) {
    // ---------- 300 族：敌人/ANM ----------
    case 300:
    case 301:
    case 304:
    case 305:
    case 309:
    case 310:
    case 311:
    case 312:
    case 321:
        return create_enemy(c, op);
    case 302: {  // anmSelect（vm+0x98 文件槽）——事件无涉
        i32 file = 0;
        if (resolve_int(c, 0, file)) e.anm_file = file;
        return 0;
    }
    case 303:   // anmScript（槽位脚本，vm+0x120 起槽数组；槽 0 = 本体）
    case 306: {  // anmRun（同槽数组，创建即跑）
        i32 slot = 0, script = 0;
        if (resolve_int(c, 0, slot) && resolve_int(c, 1, script) && slot >= 0 && slot < 4)
            e.anm_script[slot] = script;  // script < 0 = 清槽（0x41fe80）
        return 0;
    }
    case 307:
    case 308:
    case 313:
    case 316:
    case 319:
    case 554:
        return 0;  // ANM/演出族：一次性特效/镜位翻转，视觉分期接入，无事件

    // ---------- 400 族：运动 ----------
    case 400:
    case 402: {
        const u32 integral[2] = {0, 0};
        if (!emit_move(integral)) return -2;
        StageMover& m = op == 400 ? e.vel : e.pos;  // 0x41bb0d：400 → vel 子位置，402 → pos
        float x = 0, y = 0;
        if (!resolve_float(c, 0, x) || !resolve_float(c, 1, y)) {
            sim.error_ = c.error;
            return -2;
        }
        if (kKeep < x) m.pos[0] = x;
        if (kKeep < y) m.pos[1] = y;
        m.mode &= 0xFFFFFFF0u;
        return 0;
    }
    case 401:
    case 403: {
        const u32 integral[5] = {1, 1, 0, 0, 0};
        if (!emit_move(integral)) return -2;
        StageMover& m = op == 401 ? e.vel : e.pos;
        i32 dur = 0, mode = 0;
        float x = 0, y = 0;
        if (!resolve_int(c, 0, dur) || !resolve_int(c, 1, mode) || !resolve_float(c, 2, x) ||
            !resolve_float(c, 3, y)) {
            sim.error_ = c.error;
            return -2;
        }
        m.mode &= 0xFFFFFFF0u;
        if (dur < 1) {
            m.postime.duration = 0;
            return 0;
        }
        if (mode == 7 || mode == 17) {  // 模式 8 归 moveBezier（425）路径
            sim.error_ = "movePosTime interpolation mode not implemented";
            return -2;
        }
        m.postime.duration = float(dur);
        m.postime.elapsed = 0;
        m.postime.mode = mode;
        m.postime.start[0] = m.pos[0];
        m.postime.start[1] = m.pos[1];
        m.postime.start[2] = m.pos[2];
        m.postime.target[0] = kKeep < x ? x : m.pos[0];
        m.postime.target[1] = kKeep < y ? y : m.pos[1];
        m.postime.target[2] = m.pos[2];
        return 0;
    }
    case 404:
    case 406:
    case 428:
    case 430: {
        const u32 integral[2] = {0, 0};
        if (!emit_move(integral)) return -2;
        StageMover& m = (op == 404 || op == 428) ? e.vel : e.pos;  // 0x41c137
        float angle = 0, speed = 0;
        if (!resolve_float(c, 0, angle) || !resolve_float(c, 1, speed)) {
            sim.error_ = c.error;
            return -2;
        }
        if (kKeep < angle) {
            if ((e.flags & 0x100000u) && mirrored_variant(op)) angle = mirror_angle(angle);
            m.angle = wrap_angle(angle);
        }
        if (kKeep < speed) m.speed = speed;
        m.mode &= 0xFFFFFFF0u;
        return 0;
    }
    case 405:
    case 407:
    case 429:
    case 431: {
        const u32 integral[4] = {1, 1, 0, 0};
        if (!emit_move(integral)) return -2;
        StageMover& m = (op == 405 || op == 429) ? e.vel : e.pos;  // 0x41c21f
        i32 dur = 0, mode = 0;
        float angle = 0, speed = 0;
        if (!resolve_int(c, 0, dur) || !resolve_int(c, 1, mode) || !resolve_float(c, 2, angle) ||
            !resolve_float(c, 3, speed)) {
            sim.error_ = c.error;
            return -2;
        }
        if (dur < 1) {
            m.polar.duration = 0;
            return 0;
        }
        if (mode == 7 || mode == 8 || mode == 17) {
            sim.error_ = "moveVelTime interpolation mode not implemented";
            return -2;
        }
        if (kKeep < angle && (e.flags & 0x100000u) && mirrored_variant(op)) angle = mirror_angle(angle);
        float from_a = m.angle, from_b = m.speed;
        float to_a = kKeep < angle ? angle : m.angle;
        const float to_b = kKeep < speed ? speed : m.speed;
        shortest_arc(from_a, to_a);
        m.polar.setup(float(dur), mode, from_a, from_b, to_a, to_b);
        m.mode &= 0xFFFFFFF2u;  // 清插值挂接标志（保持其余位）
        return 0;
    }
    case 408:
    case 410: {  // moveCircle（0x41c471）
        const u32 integral[4] = {0, 0, 0, 0};
        if (!emit_move(integral)) return -2;
        StageMover& m = (op == 408) ? e.vel : e.pos;
        float a0 = 0, a1 = 0, a2 = 0, a3 = 0;
        if (!resolve_float(c, 0, a0) || !resolve_float(c, 1, a1) || !resolve_float(c, 2, a2) ||
            !resolve_float(c, 3, a3)) {
            sim.error_ = c.error;
            return -2;
        }
        if ((m.mode & 0xFu) != 2) {
            m.aux[0] = m.pos[0];
            m.aux[1] = m.pos[1];
            m.aux[2] = m.pos[2];
        }
        if (kKeep < a0) m.angle = wrap_angle(a0);
        if (kKeep < a1) m.speed = a1;
        if (kKeep < a2) m.radius = a2;
        if (kKeep < a3) m.growth = a3;
        m.mode = (m.mode & 0xFFFFFFF2u) | 2u;
        // 0x41c56b→0x473a10：mode4 时 pos[3..5]=pos[0..2]，随即 0x473780
        // 累加一次（mode2 子位置 = 圆心 + polar(angle, radius) + 量化）；
        // 0x41c572→0x419950：主位置立即同步（指令内生效，不等下帧积分）。
        {
            float ox = 0, oy = 0;
            polar_xy(m.angle, m.radius, ox, oy);
            m.pos[0] = m.aux[0] + ox;
            m.pos[1] = m.aux[1] + oy;
            m.pos[2] = m.aux[2] + 0.0f;
            m.pos[0] = round001(m.pos[0]);
            m.pos[1] = round001(m.pos[1]);
            for (int k = 0; k < 3; ++k) {
                const float delta = (e.vel.pos[k] + e.pos.pos[k]) - e.position[k];
                e.position[k] = e.position[k] + delta;
            }
            e.position[0] = round001(e.position[0]);
            e.position[1] = round001(e.position[1]);
        }
        return 0;
    }
    case 409:
    case 411: {  // moveCircleTime（0x41c57c）：(dur, mode, speed_to, radius, growth)
        // 0x41c5b2..0x41c77d：5 参——speed 插值 from=当前值 to=参数 2；
        // radius（参数 3）/growth（参数 4）keep 语义直设；不设圆心 aux（408 已设）。
        // 行为实证（oracle 逐帧）：v.s 每帧向参数 2 线性逼近、v.r 恒为参数 3。
        const u32 integral[5] = {1, 1, 0, 0, 0};
        if (!emit_move(integral)) return -2;
        StageMover& m = (op == 409) ? e.vel : e.pos;
        i32 dur = 0, mode = 0;
        float speed_to = 0, radius = 0, growth = 0;
        if (!resolve_int(c, 0, dur) || !resolve_int(c, 1, mode) ||
            !resolve_float(c, 2, speed_to) || !resolve_float(c, 3, radius) ||
            !resolve_float(c, 4, growth)) {
            sim.error_ = c.error;
            return -2;
        }
        if (dur < 1) {
            m.polar.duration = 0;
        } else {
            if (mode == 7 || mode == 8 || mode == 17) {
                sim.error_ = "moveCircleTime interpolation mode not implemented";
                return -2;
            }
            // 0x41c627 fldz：角维 from=to=0.0（恒零）——0x425 尾清 mover mode 后
            // 0x419450 每帧把插值角维（0）写回 vel.angle（实测 MBoss 归零）。
            // speed 维 from=当前值，to=参数 2。
            const float from_b = m.speed;
            const float to_b = kKeep < speed_to ? speed_to : m.speed;
            m.polar.setup(float(dur), mode, 0.0f, from_b, 0.0f, to_b);
        }
        if (kKeep < radius) m.radius = radius;
        if (kKeep < growth) m.growth = growth;
        m.mode = (m.mode & 0xFFFFFFF2u) | 2u;
        return 0;
    }
    case 432:
    case 433: {  // moveEnm（0x41cd0c/0x41cd1f）：子位置 = 目标敌主位置
        const u32 integral[1] = {1};
        if (!emit_move(integral)) return -2;
        i32 id = 0;
        if (!resolve_int(c, 0, id)) {
            sim.error_ = c.error;
            return -2;
        }
        StageEnemy* target = sim.find_by_script_id(id);
        StageMover& m = (op == 432) ? e.vel : e.pos;
        if (!target) {
            // 0x417f40 id 0/未命中链尾：id 0 返回 null 后 0x41ccb1/0x41cce3
            // 仍解引用（[0x1230..]——oracle bump 零页读 0），子位置清零。
            m.pos[0] = 0.0f;
            m.pos[1] = 0.0f;
            m.pos[2] = 0.0f;
            return 0;
        }
        m.pos[0] = target->position[0];
        m.pos[1] = target->position[1];
        m.pos[2] = target->position[2];
        return 0;
    }
    case 424: {  // moveSetMirror（0x41b30c）：bit20 = arg&1
        i32 value = 0;
        if (!resolve_int(c, 0, value)) {
            sim.error_ = c.error;
            return -2;
        }
        e.flags = (e.flags & ~0x100000u) | ((u32(value) << 20) & 0x100000u);
        return 0;
    }
    case 425: {  // moveBezier（0x41bfb1 → 0x422f80 模式 8）
        const u32 integral[7] = {1, 0, 0, 0, 0, 0, 0};
        if (!emit_move(integral)) return -2;
        StageMover& m = e.vel;
        i32 dur = 0;
        float x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
        if (!resolve_int(c, 0, dur) || !resolve_float(c, 1, x1) || !resolve_float(c, 2, y1) ||
            !resolve_float(c, 3, x2) || !resolve_float(c, 4, y2) || !resolve_float(c, 5, x3) ||
            !resolve_float(c, 6, y3)) {
            sim.error_ = c.error;
            return -2;
        }
        m.mode &= 0xFFFFFFF0u;
        if (dur < 1) {
            m.postime.duration = 0;
            return 0;
        }
        m.postime.duration = float(dur);
        m.postime.elapsed = 0;
        m.postime.mode = 8;
        m.postime.start[0] = m.pos[0];
        m.postime.start[1] = m.pos[1];
        m.postime.start[2] = m.pos[2];
        m.postime.target[0] = kKeep < x2 ? x2 : m.pos[0];
        m.postime.target[1] = kKeep < y2 ? y2 : m.pos[1];
        m.postime.target[2] = 0.0f;
        m.postime.p1[0] = x1;
        m.postime.p1[1] = y1;
        m.postime.p1[2] = 0.0f;
        m.postime.p2[0] = x3;
        m.postime.p2[1] = y3;
        m.postime.p2[2] = 0.0f;
        return 0;
    }
    case 412: {  // moveRand（0x41cdc2）：按象限选随机角后走 moveVelTime 同款挂接
        const u32 integral[3] = {1, 1, 0};
        if (!emit_move(integral)) return -2;
        i32 dur = 0, mode = 0;
        float speed = 0;
        if (!resolve_int(c, 0, dur) || !resolve_int(c, 1, mode) || !resolve_float(c, 2, speed)) {
            sim.error_ = c.error;
            return -2;
        }
        const float qx = e.limit_half[0] * 0.25f;
        const float qy = e.limit_half[1] * 0.25f;
        float angle = 0;
        if (e.position[0] >= e.limit_center[0] - qx) {
            if (e.position[0] > e.limit_center[0] + qx) {
                // 右缘：向左 120° 扇
                angle = wrap_angle(float(sim.random.signed_unit() * 3.1415927410125732 / 3.0 +
                                         3.1415927410125732));
            } else {
                const float base = float(sim.random.signed_unit() * 3.1415927410125732);
                const float quarter = base * 0.25f;
                const bool flip = (u32(sim.random.step16()) % 3u) != 0;
                const bool right_of_player = sim.player_x <= e.position[0];
                angle = (flip == right_of_player) ? quarter + 3.1415927f : quarter;
            }
        } else {
            // 左缘：向右 120° 扇
            angle = float(sim.random.signed_unit() * 3.1415927410125732 / 3.0);
        }
        // 上下缘翻转与近垂直修正（0x41cdc2 后半）
        if (e.position[1] >= e.limit_center[1] - qy) {
            if (e.position[1] > e.limit_center[1] + qy)
                angle = angle < 0 ? angle : -angle;
        } else {
            angle = angle < 0 ? -angle : angle;
        }
        // 近垂直修正（先查 -π/2 正上，再查 +π/2 正下；常量为原版字面）
        if (std::fabs(angle + 1.5707964f) < 0.05f) {
            angle = angle >= -1.5707964f ? -1.5207964f : -1.6207963f;
        } else if (std::fabs(angle - 1.5707964f) < 0.05f) {
            angle = angle >= 1.5707964f ? 1.6207963f : 1.5207964f;
        }
        StageMover& m = e.vel;
        if (dur < 1) {
            m.polar.duration = 0;
            return 0;
        }
        if (mode == 7 || mode == 8 || mode == 17) {
            sim.error_ = "moveRand interpolation mode not implemented";
            return -2;
        }
        float from_a = m.angle, from_b = m.speed;
        float to_a = angle, to_b = speed;
        shortest_arc(from_a, to_a);
        m.polar.setup(float(dur), mode, from_a, from_b, to_a, to_b);
        m.mode &= 0xFFFFFFF2u;
        return 0;
    }
    case 434:
    case 435:
    case 436:
    case 437:
    case 438:
    case 439:
        sim.error_ = "moveCurve family not implemented in stage1 window";
        return -2;
    case 427: {  // moveReset（0x41ba18）：清 mover 与累积量，主位置保持
        e.pos.pos[0] = e.pos.pos[1] = e.pos.pos[2] = 0;
        e.vel.pos[0] = e.position[0];
        e.vel.pos[1] = e.position[1];
        e.vel.pos[2] = e.position[2];
        for (StageMover* m : {&e.vel, &e.pos}) {
            m->speed = m->angle = m->radius = m->growth = 0;
            m->aux[0] = m->aux[1] = m->aux[2] = 0;
            m->mode = 0;
            m->polar.duration = 0;
            m->postime.duration = 0;
        }
        return 0;
    }

    // ---------- 500 族：属性/关卡事件 ----------
    case 500: {
        float w = 0, h = 0;
        if (resolve_float(c, 0, w) && resolve_float(c, 1, h)) {
            e.hurtbox[0] = w;
            e.hurtbox[1] = h;
            return 0;
        }
        sim.error_ = c.error;
        return -2;
    }
    case 501:
        return 0;  // setHitbox：无事件
    case 502: {  // flagSet：flags |= arg（0x41d231）
        i32 mask = 0;
        if (!resolve_int(c, 0, mask)) {
            sim.error_ = c.error;
            return -2;
        }
        e.flags |= u32(mask);
        return 0;
    }
    case 503: {
        i32 mask = 0;
        if (!resolve_int(c, 0, mask)) {
            sim.error_ = c.error;
            return -2;
        }
        e.flags &= ~u32(mask);
        return 0;
    }
    case 504: {  // moveLimit（0x41cd32）
        float cx = 0, cy = 0, w = 0, h = 0;
        if (resolve_float(c, 0, cx) && resolve_float(c, 1, cy) && resolve_float(c, 2, w) &&
            resolve_float(c, 3, h)) {
            e.flags |= 0x40000u;
            e.limit_center[0] = cx;
            e.limit_center[1] = cy;
            e.limit_half[0] = w;
            e.limit_half[1] = h;
            return 0;
        }
        sim.error_ = c.error;
        return -2;
    }
    case 505:
        e.flags &= ~0x40000u;
        return 0;
    case 506:
    case 507:
    case 508:
    case 509:
    case 510:
        return 0;  // drop 族：无事件
    case 511: {  // lifeSet（0x41d34c：整型解析，×3 槽 + 0x3f58=7×life）
        i32 life = 0;
        if (resolve_int(c, 0, life)) {
            e.life = float(life);
            if (sim.replay_events) {
                const i32 args[1] = {life};
                sim.sink.boss_op(e.serial, 511, args, 1, "");
            }
            return 0;
        }
        sim.error_ = c.error;
        return -2;
    }
    case 512: {  // setBoss（0x41d395）：slot = arg0，负参撤销自身槽
        i32 slot = 0;
        if (!resolve_int(c, 0, slot)) {
            sim.error_ = c.error;
            return -2;
        }
        if (slot >= 0 && slot < 3) {
            sim.boss_slot[slot] = e.script_id;
            e.flags |= 0x1000000u;  // 0x41d3d8：BOSS 位（525 清场豁免）
        } else {
            for (i32& entry : sim.boss_slot)
                if (entry == e.script_id) entry = -1;
            e.flags &= ~0x1000000u;
        }
        if (sim.replay_events) {
            const i32 args[1] = {slot};
            sim.sink.boss_op(e.serial, 512, args, 1, "");
        }
        return 0;
    }
    case 558: {  // 0x41e451：flags bit20（镜像）按参数异或翻转
        i32 value = 0;
        if (!resolve_int(c, 0, value)) {
            sim.error_ = c.error;
            return -2;
        }
        e.flags = (e.flags & ~0x100000u) |
                  ((e.flags ^ (u32(value) << 20)) & 0x100000u);
        return 0;
    }
    case 513:
        // Boss 第一非符锚点（§7.3）：MainBoss 创建的 "Boss" 敌机首条 513。
        if (e.serial == sim.boss_main_serial && sim.boss_nonspell_frame < 0)
            sim.boss_nonspell_frame = i32(sim.frame);
        if (sim.replay_events) sim.sink.boss_op(e.serial, 513, nullptr, 0, "");
        return 0;
    case 515:
    case 516:
    case 517:
    case 523:
    case 524:
    case 527:
    case 540:
    case 542:
    case 545:
    case 557:
    case 559:
    case 560:
    case 562:
    case 638:
        return 0;  // 计时/音效/符卡/掉落等：无事件
    case 514: {  // setInterrupt（0x41e262）：{t1,t2,name} 记入 vm+0x5234+id*16
        i32 id = 0;
        if (!resolve_int(c, 0, id)) {
            sim.error_ = c.error;
            return -2;
        }
        i32 t1 = 0, t2 = 0;
        resolve_int(c, 1, t1);
        resolve_int(c, 2, t2);
        // 名字参数（arg3 起的 {len, name} 尾串——指令名区原样）
        std::string name;
        const EclInstruction* i = c.instruction;
        if (i->parameter_count > 3) {
            const u32 length = u32(i->argument<i32>(3));
            const char* bytes = reinterpret_cast<const char*>(i) + 16 + 4 * 4;
            name.assign(bytes, strnlen(bytes, length ? length - 1 : 0));
        }
        if (id == 0) e.interrupt_label = name;  // 525 中断跳转用（id 0）
        if (sim.replay_events) {
            const i32 args[3] = {id, t1, t2};
            sim.sink.boss_op(e.serial, 514, args, 3, name.c_str());
        }
        return 0;
    }
    case 521: {  // 死亡中断标签（0x41e33f：写 vm+0x5240+id*16）
        i32 id = 0;
        if (!resolve_int(c, 0, id)) {
            sim.error_ = c.error;
            return -2;
        }
        std::string name;
        const EclInstruction* i = c.instruction;
        if (i->parameter_count > 1) {
            const u32 length = u32(i->argument<i32>(1));
            const char* bytes = reinterpret_cast<const char*>(i) + 16 + 4 * 2;
            name.assign(bytes, strnlen(bytes, length ? length - 1 : 0));
        }
        if (sim.replay_events) {
            const i32 args[1] = {id};
            sim.sink.boss_op(e.serial, 521, args, 1, name.c_str());
        }
        return 0;
    }
    case 518:   // dialogRead（0x41e3da）：对话本体缺席，共享 525 的清场尾块
    case 525: {  // enmKillAll（0x41e3fd → 0x41ac70）
        // 0x41ac70：flags 门控销毁——(bit5/7 全清 且 bit 0x400/0x800000/0x1000000
        // 全清) 或 bit8 置位。命中者当场走 0x41aad0（0x417b10 开头抽 1 次
        // signed_unit；合成 ANM 态槽弹数为 0，无每弹消耗）并标 0x4000000，
        // 摘除由 0x418e00 遍历完成（无 RNG）。main/MapleEnemy/EtBreak 的
        // ins_502(32)（bit5）豁免清场；BOSS 位（0x1000000，512 setBoss）豁免。
        for (auto& entry : sim.enemies) {
            const u32 f = entry->flags;
            const bool kill = (((f & 0xa0u) == 0) && ((f & 0x1800400u) == 0)) ||
                              ((f & 0x100u) != 0);
            if (kill) {
                sim.random.signed_unit();
                entry->dead = true;
            }
        }
        return 0;
    }
    case 519:
        // dialogWait（0x41e407）：msgmgr+0x198 == 0 即通过。oracle 侧合成
        // msgmgr 恒 0 → 即过；此处同判（对话系统缺席，§7.1【待校准】）。
        return sim.boss_dialog_wait();
    case 520:  // bossWait（0x41e42e）：任一 BOSS 槽存活则让出
        return sim.boss() ? 1 : 0;
    case 535: {  // diffI（0x41d439）：dest = args[1 + DIFF]
        i32 value = 0;
        if (!resolve_int(c, 1 + sim.diff, value)) {
            sim.error_ = c.error;
            return -2;
        }
        i32* dest = c.integer_destination(0);
        if (!dest) {
            sim.error_ = c.error.empty() ? "diffI destination" : c.error;
            return -2;
        }
        *dest = value;
        return 0;
    }
    case 536: {
        float value = 0;
        if (!resolve_float(c, 1 + sim.diff, value)) {
            sim.error_ = c.error;
            return -2;
        }
        float* dest = c.float_destination(0);
        if (!dest) {
            sim.error_ = c.error.empty() ? "diffF destination" : c.error;
            return -2;
        }
        *dest = value;
        return 0;
    }
    case 548: {  // diffWait（0x41f2c4）：time -= args[DIFF]（0..3 四难度，无 dest）
        i32 frames = 0;
        if (!resolve_int(c, sim.diff, frames)) {
            sim.error_ = c.error;
            return -2;
        }
        c.time -= float(frames);
        return 0;
    }
    case 555: {  // enmAlive（0x41f4f3）
        i32 id = 0;
        if (!resolve_int(c, 1, id)) {
            sim.error_ = c.error;
            return -2;
        }
        i32* dest = c.integer_destination(0);
        if (!dest) {
            sim.error_ = c.error.empty() ? "enmAlive destination" : c.error;
            return -2;
        }
        *dest = sim.find_by_script_id(id) ? 1 : 0;
        return 0;
    }
    case 561:  // die（0x41b437 含死亡环弹，环弹不在事件集）——敌机死亡
        e.dead = true;
        return -1;

    // ---------- 600 族：弹幕发射器 ----------
    case 600: {  // etNew（0x41d573）
        i32 slot = 0;
        if (!resolve_int(c, 0, slot) || slot < 0 || slot >= 8) {
            sim.error_ = "etNew slot";
            return -2;
        }
        e.et[slot].reset();
        return 0;
    }
    case 601:  // etOn（0x41d6b1 → 0x40ffe0 → 0x40e840）
        return fire_et(c, e);
    case 602: {
        i32 slot = 0, s1 = 0, s2 = 0;
        if (resolve_int(c, 0, slot) && resolve_int(c, 1, s1) && resolve_int(c, 2, s2) && slot >= 0 &&
            slot < 8) {
            e.et[slot].sprite = u32(s1);
            e.et[slot].subtype = u32(s2);
            return 0;
        }
        sim.error_ = "etSprite args";
        return -2;
    }
    case 603: {
        i32 slot = 0;
        float x = 0, y = 0;
        if (resolve_int(c, 0, slot) && resolve_float(c, 1, x) && resolve_float(c, 2, y) && slot >= 0 &&
            slot < 8) {
            e.et[slot].offset[0] = x;
            e.et[slot].offset[1] = y;
            return 0;
        }
        sim.error_ = "etOffset args";
        return -2;
    }
    case 604: {
        i32 slot = 0;
        float a = 0, b = 0;
        if (resolve_int(c, 0, slot) && resolve_float(c, 1, a) && resolve_float(c, 2, b) && slot >= 0 &&
            slot < 8) {
            e.et[slot].angle1 = a;
            e.et[slot].angle2 = b;
            return 0;
        }
        sim.error_ = "etAngle args";
        return -2;
    }
    case 605: {
        i32 slot = 0;
        float a = 0, b = 0;
        if (resolve_int(c, 0, slot) && resolve_float(c, 1, a) && resolve_float(c, 2, b) && slot >= 0 &&
            slot < 8) {
            e.et[slot].speed1 = a;
            e.et[slot].speed2 = b;
            return 0;
        }
        sim.error_ = "etSpeed args";
        return -2;
    }
    case 606: {
        i32 slot = 0, count = 0, ways = 0;
        if (resolve_int(c, 0, slot) && resolve_int(c, 1, count) && resolve_int(c, 2, ways) &&
            slot >= 0 && slot < 8) {
            e.et[slot].count = i16(count);
            e.et[slot].ways = i16(ways);
            return 0;
        }
        sim.error_ = "etCount args";
        return -2;
    }
    case 607: {
        i32 slot = 0, mode = 0;
        if (resolve_int(c, 0, slot) && resolve_int(c, 1, mode) && slot >= 0 && slot < 8) {
            e.et[slot].aim = i16(mode);
            return 0;
        }
        sim.error_ = "etAim args";
        return -2;
    }
    case 608:
    case 609:
    case 610:
    case 611:
    case 612:
        return 0;  // etSound/etEx：仅存储（出生后行为，不在事件集）
    case 613:
        for (auto& slot : e.et) slot.reset();
        return 0;
    case 614: {  // etCopy（0x41d61a）
        i32 to = 0, from = 0;
        if (resolve_int(c, 0, to) && resolve_int(c, 1, from) && to >= 0 && to < 8 && from >= 0 &&
            from < 8) {
            e.et[to] = e.et[from];
            return 0;
        }
        sim.error_ = "etCopy args";
        return -2;
    }
    case 615:
    case 616:
        return 0;  // etCancel/Clear：弹体不在事件集
    case 624: {  // etSpeedD（0x41da32）：按 DIFF 选参
        i32 slot = 0;
        if (!resolve_int(c, 0, slot) || slot < 0 || slot >= 8) {
            sim.error_ = "etSpeedD slot";
            return -2;
        }
        float s1 = 0, s2 = 0;
        if (!resolve_float(c, 1 + sim.diff, s1) || !resolve_float(c, 5 + sim.diff, s2)) {
            sim.error_ = c.error;
            return -2;
        }
        e.et[slot].speed1 = s1;
        e.et[slot].speed2 = s2;
        return 0;
    }
    case 625: {  // etCountD（0x41dad7）
        i32 slot = 0;
        if (!resolve_int(c, 0, slot) || slot < 0 || slot >= 8) {
            sim.error_ = "etCountD slot";
            return -2;
        }
        i32 count = 0, ways = 0;
        if (!resolve_int(c, 1 + sim.diff, count) || !resolve_int(c, 5 + sim.diff, ways)) {
            sim.error_ = c.error;
            return -2;
        }
        e.et[slot].count = i16(count);
        e.et[slot].ways = i16(ways);
        return 0;
    }
    case 629:
    case 632:
        return 0;  // fog/funcSet：无事件

    // ---------- 700/800/900/1000 族 ----------
    case 700:
    case 701:
    case 702:
    case 703:
    case 704:
    case 705:
    case 706:
    case 707:
    case 708:
    case 709:
    case 710:
    case 711:
    case 713:
    case 714:
        // 激光族：stage1 事件集未覆盖激光弹（合同 §4.3），此处仅放行脚本流程。
        return 0;
    case 900:
        return 0;  // 原版落默认分支 no-op（ecl-vm.md §4.2 900）
    case 1000:
    case 1001:
    case 1002:
    case 1003:
        return 0;

    default:
        sim.error_ = "unimplemented game opcode " + std::to_string(op);
        return -2;
    }
}

// enmCreate 族（0x41fcc0 → 0x4183f0）。
int StageCommands::create_enemy(EclContext& c, u16 op) {
    StageSim& sim = *this->sim;
    StageEnemy& e = *sim.current;
    if (op >= 309 && op <= 312 && sim.boss()) return 0;  // BOSS 槽空时才建（0x41b2dd）
    const EclInstruction* ins = c.instruction;
    const u32 length = u32(ins->argument<i32>(0));  // 含 NUL 与 4 对齐
    const char* name = reinterpret_cast<const char*>(ins) + 16 + 4;
    std::string routine(name, strnlen(name, length ? length - 1 : 0));

    float x = 0, y = 0;
    if (!resolve_postname_float(c, 0, 1, x) || !resolve_postname_float(c, 1, 2, y)) {
        sim.error_ = c.error;
        return -2;
    }
    i32 life = 0;
    if (!resolve_postname_int(c, 2, 3, life)) {
        sim.error_ = c.error;
        return -2;
    }
    const bool relative = op == 300 || op == 309 || op == 321 || op == 311 || op == 304;
    // 0x41fcc0:60-62：311/304/312/305（0x137/0x130/0x138/0x131）产 param2[6]=1
    // ——镜像位源；0x41fcc0:63-66 创建者已镜像时翻转该位并 x 取负；
    // 0x4183f0:28 以 xor-and 把 param2[6]<<20 无条件写入新敌机 flags bit20。
    bool mirror = op == 311 || op == 304 || op == 312 || op == 305;
    if (relative) {
        x = x + e.position[0];
        y = y + e.position[1];
    }
    if (e.flags & 0x100000u) {
        mirror = !mirror;
        x = x * -1.0f;
    }
    StageEnemy* created = sim.spawn_enemy(routine.c_str(), x, y, float(life), &e, false,
                                          mirror);
    return 0;
}

// etOn（0x41d6b1 + 0x40ffe0 + 0x40e840）：按槽状态合成出生参数逐发产事件。
int StageCommands::fire_et(EclContext& c, StageEnemy& e) {
    StageSim& sim = *this->sim;
    i32 slot = 0;
    if (!resolve_int(c, 0, slot) || slot < 0 || slot >= 8) {
        sim.error_ = "etOn slot";
        return -2;
    }
    StageEt& et = e.et[slot];
    // 出生点：absolute[2] <= 0.9 → 相对（偏移 + 敌机主位置）；否则绝对 + 偏移。
    if (et.absolute[2] <= 0.9f) {
        et.spawn[0] = et.offset[0] + e.position[0];
        et.spawn[1] = et.offset[1] + e.position[1];
        et.spawn[2] = et.offset[2] + e.position[2];
    } else {
        et.spawn[0] = et.absolute[0] + et.offset[0];
        et.spawn[1] = et.absolute[1] + et.offset[1];
        et.spawn[2] = et.absolute[2] + et.offset[2];
    }
    // 瞄准角（0x443f60：出生点 → 玩家，重合取 π/2）。
    const float dy = sim.player_y - et.spawn[1];
    const float dx = sim.player_x - et.spawn[0];
    const float aim = (dy == 0.0f && dx == 0.0f)
                          ? 1.5707964f
                          : float(std::atan2(static_cast<double>(dy), static_cast<double>(dx)));

    const i32 ways = et.ways;
    const i32 count = et.count;
    const i32 mode = et.aim;
    // 0x40e840 角合成全程 x87 80 位（double 近似）；f32 常量提升后参与：
    // 6.2831855f = 6.283185482025146484375、3.1415927f = 3.1415927410125732421875。
    const double kTwoPiF = static_cast<double>(6.2831855f);
    const double kPiF = static_cast<double>(3.1415927f);
    const double aim_d = aim;
    for (i32 way = 0; way < ways; ++way) {
        // 路间速率（0x40e840 开头，80 位中间 → f32 落点 node+0xb88）。
        float speed = et.speed1;
        if (ways >= 2)
            speed = static_cast<float>(et.speed1 - (et.speed1 - et.speed2) *
                                                      static_cast<double>(way) /
                                                      static_cast<double>(ways - 1));
        for (i32 idx = 0; idx < count; ++idx) {
            double angle = 0;
            switch (mode) {
            case 0:
            case 1: {
                if ((count & 1) == 0)
                    angle = et.angle2 * 0.5 + static_cast<double>(idx / 2) * et.angle2;
                else
                    angle = static_cast<double>((idx + 1) / 2) * et.angle2;
                angle = angle + 0.0;
                if (idx & 1) angle = angle * -1.0;
                if (mode == 0) angle = angle + aim_d;
                angle = et.angle1 + angle;
                break;
            }
            case 2:
            case 3:
                // 0x40e950..0x40e988 精确求值序：aim+0 落 f32 → idx*2π/count 相加后
                // 再落 f32 → 与 80 位的 angle2*way+angle1 求和 → 尾部双重 wrap。
                {
                    const float base = mode == 2 ? aim : 0.0f;
                    const double spoke = static_cast<double>(idx) * kTwoPiF /
                                         static_cast<double>(count);
                    const float mid = static_cast<float>(spoke + base);
                    angle = (et.angle2 * static_cast<double>(way) + et.angle1) +
                            static_cast<double>(mid);
                }
                break;
            case 4:
                angle = aim_d;
                angle = et.angle2 * static_cast<double>(way) + et.angle1 +
                        static_cast<double>(idx) * kTwoPiF / static_cast<double>(count) +
                        kPiF / static_cast<double>(count) + angle;
                break;
            case 5:
                angle = et.angle2 * static_cast<double>(way) + et.angle1 +
                        static_cast<double>(idx) * kTwoPiF / static_cast<double>(count) +
                        kPiF / static_cast<double>(count);
                break;
            case 6:
                angle = et.angle2 +
                        sim.random.unit() * static_cast<double>(et.angle1 - et.angle2);
                break;
            case 7:
                speed = static_cast<float>(sim.random.unit() *
                                               static_cast<double>(et.speed1 - et.speed2) +
                                           et.speed2);
                angle = et.angle2 * static_cast<double>(way) + et.angle1 +
                        static_cast<double>(idx) * kTwoPiF / static_cast<double>(count);
                break;
            case 8:
                angle = et.angle2 +
                        sim.random.unit() * static_cast<double>(et.angle1 - et.angle2);
                speed = static_cast<float>(sim.random.unit() *
                                               static_cast<double>(et.speed1 - et.speed2) +
                                           et.speed2);
                break;
            case 9:
            case 10: {
                if ((count & 1) == 0)
                    angle = et.angle2 * 0.5 + static_cast<double>(way / 2) * et.angle2;
                else
                    angle = static_cast<double>((way + 1) / 2) * et.angle2;
                angle = angle + static_cast<double>(idx) * kTwoPiF / static_cast<double>(count);
                if (way & 1) angle = angle * -1.0;
                if (mode == 9) angle = angle + aim_d;
                angle = et.angle1 + angle;
                break;
            }
            default:
                angle = et.angle1;
                break;
            }
            // 0x40e840 尾：FUN_00472e90(acc, 0)（传参落 f32）→ FUN_00472f30（再落）。
            const float stored = wrap_angle(wrap_angle(static_cast<float>(angle)));
            float x = et.spawn[0], y = et.spawn[1];
            // z：0x40e840 写入 spawn z 后无条件覆写 0x3dcccccd（渲染层常量），
            // 事件口径恒 0.1f；spin（et[9]）只作用于 x/y（0x40e840 尾段）。
            const float z = 0.1f;
            if (et.spin != 0.0f) {
                float ox = 0, oy = 0;
                polar_xy(stored, et.spin, ox, oy);
                x = x + ox;
                y = y + oy;
            }
            sim.sink.bullet_spawn(e.serial, u32(slot), u32(way), u32(idx), x, y, z, stored, speed,
                                  et.sprite, et.subtype);
            if (sim.interactive) {
                // 交互模式同源产弹体实体（直进；无 RNG、无事件，契约不受影响）。
                StageBullet bullet;
                bullet.x = x;
                bullet.y = y;
                bullet.z = z;
                bullet.angle = stored;
                bullet.speed = speed;
                bullet.sprite = et.sprite;
                bullet.subtype = et.subtype;
                sim.bullets.push_back(bullet);
            }
        }
    }
    return 0;
}

}  // namespace th13
