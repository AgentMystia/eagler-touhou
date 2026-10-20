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
            float literal;
            std::memcpy(&literal, reinterpret_cast<const u8*>(ins) + 16 + i * 4, 4);
            if (integral && integral[i]) {
                i32 v = 0;
                if (!c.integer(i, i32(literal), v, false) && c.error.empty()) c.error = "move arg resolve";
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
        return 0;
    }
    case 303:
    case 306:
    case 307:
    case 308:
    case 313:
    case 316:
    case 319:
    case 554:
        return 0;  // ANM/演出族：无事件

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
        return 0;
    }
    case 409:
    case 411: {  // moveCircleTime（0x41c57c）
        const u32 integral[6] = {1, 1, 0, 0, 0, 0};
        if (!emit_move(integral)) return -2;
        StageMover& m = (op == 409) ? e.vel : e.pos;
        i32 dur = 0, mode = 0;
        float a0 = 0, a1 = 0, a2 = 0, a3 = 0;
        if (!resolve_int(c, 0, dur) || !resolve_int(c, 1, mode) || !resolve_float(c, 2, a0) ||
            !resolve_float(c, 3, a1) || !resolve_float(c, 4, a2) || !resolve_float(c, 5, a3)) {
            sim.error_ = c.error;
            return -2;
        }
        if ((m.mode & 0xFu) != 2) {
            m.aux[0] = m.pos[0];
            m.aux[1] = m.pos[1];
            m.aux[2] = m.pos[2];
        }
        if (dur < 1) {
            m.polar.duration = 0;
        } else {
            if (mode == 7 || mode == 8 || mode == 17) {
                sim.error_ = "moveCircleTime interpolation mode not implemented";
                return -2;
            }
            float from_a = m.angle, from_b = m.speed;
            float to_a = kKeep < a0 ? a0 : m.angle;
            const float to_b = kKeep < a1 ? a1 : m.speed;
            shortest_arc(from_a, to_a);
            m.polar.setup(float(dur), mode, from_a, from_b, to_a, to_b);
        }
        if (kKeep < a2) m.radius = a2;
        if (kKeep < a3) m.growth = a3;
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
        if (!target) {
            sim.error_ = "moveEnm target not found (original would dereference null)";
            return -2;
        }
        StageMover& m = (op == 432) ? e.vel : e.pos;
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
    case 511: {  // lifeSet（×4 槽）
        float life = 0;
        if (resolve_float(c, 0, life)) {
            e.life = life;
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
        } else {
            for (i32& entry : sim.boss_slot)
                if (entry == e.script_id) entry = -1;
        }
        return 0;
    }
    case 513:
    case 515:
    case 516:
    case 517:
    case 521:
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
    case 514: {  // setInterrupt（0x41e262）：记录 id 0 标签（525 中断跳转用）
        i32 id = 0;
        if (!resolve_int(c, 0, id)) {
            sim.error_ = c.error;
            return -2;
        }
        if (id == 0) {
            // 名字参数（arg4 起）——按调用类尾部取字符串
            const EclInstruction* i = c.instruction;
            if (i->parameter_count > 3) {
                const u32 length = u32(i->argument<i32>(3));  // {len, name} 跟在前 3 个整型后
                const char* name = reinterpret_cast<const char*>(i) + 16 + 4 * 4;
                e.interrupt_label.assign(name, strnlen(name, length ? length - 1 : 0));
            }
        }
        return 0;
    }
    case 518:   // dialogRead（0x41e3da）：对话本体缺席，但共享 525 的清场+全体中断尾块
    case 525: {  // enmKillAll（0x41e3fd → 0x41ac70）
        for (auto& entry : sim.enemies) {
            if (entry.get() == &e || entry->dead) continue;
            if (!entry->interrupt_label.empty()) {
                const i32 index = sim.program.find_index(entry->interrupt_label.c_str());
                if (index >= 0)
                    entry->threads->restart(entry->interrupt_label.c_str(),
                                            entry->threads->main.difficulty);
                else
                    entry->dead = true;
            } else {
                entry->dead = true;
            }
        }
        return 0;
    }
    case 519:
        return 1;  // dialogWait：无对话系统，让出（等待永不结束）
    case 520:  // bossWait（0x41e42e）：任一 BOSS 槽存活则让出
        return sim.boss() ? 1 : 0;
    case 535: {  // diffI（0x41d439）：dest = args[1 + DIFF]
        i32 value = 0;
        if (!resolve_int(c, 1 + 3, value)) {
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
        if (!resolve_float(c, 1 + 3, value)) {
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
        if (!resolve_int(c, 3, frames)) {
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
        if (!resolve_float(c, 1 + 3, s1) || !resolve_float(c, 5 + 3, s2)) {
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
        if (!resolve_int(c, 1 + 3, count) || !resolve_int(c, 5 + 3, ways)) {
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
        sim.error_ = "unimplemented game opcode";
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
    const bool inherit = op == 311 || op == 304 || op == 312 || op == 305;
    if (relative) {
        x = x + e.position[0];
        y = y + e.position[1];
    }
    (void)inherit;  // ANM 遗产标志只影响视觉继承
    if (e.flags & 0x100000u) {
        x = x * -1.0f;
    }
    sim.spawn_enemy(routine.c_str(), x, y, float(life), &e, inherit);
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
    const float two_pi = 6.2831855f;
    for (i32 way = 0; way < ways; ++way) {
        // 路间速率（0x40e840 开头）。
        float speed = et.speed1;
        if (ways >= 2) speed = et.speed1 - (et.speed1 - et.speed2) * float(way) / float(ways - 1);
        for (i32 idx = 0; idx < count; ++idx) {
            float angle = 0;
            switch (mode) {
            case 0:
            case 1: {
                if ((count & 1) == 0)
                    angle = et.angle2 * 0.5f + float(idx / 2) * et.angle2;
                else
                    angle = float((idx + 1) / 2) * et.angle2;
                angle = angle + 0.0f;
                if (idx & 1) angle = angle * -1.0f;
                if (mode == 0) angle = angle + aim;
                angle = et.angle1 + angle;
                break;
            }
            case 2:
                angle = aim;
                angle = et.angle2 * float(way) + et.angle1 + (float(idx) * two_pi) / float(count) + angle;
                break;
            case 3:
                angle = et.angle2 * float(way) + et.angle1 + (float(idx) * two_pi) / float(count);
                break;
            case 4:
                angle = aim;
                angle = et.angle2 * float(way) + et.angle1 + (float(idx) * two_pi) / float(count) +
                        3.1415927f / float(count) + angle;
                break;
            case 5:
                angle = et.angle2 * float(way) + et.angle1 + (float(idx) * two_pi) / float(count) +
                        3.1415927f / float(count);
                break;
            case 6:
                angle = et.angle2 + sim.random.unit() * (et.angle1 - et.angle2);
                break;
            case 7:
                speed = sim.random.unit() * (et.speed1 - et.speed2) + et.speed2;
                angle = et.angle2 * float(way) + et.angle1 + (float(idx) * two_pi) / float(count);
                break;
            case 8:
                angle = et.angle2 + sim.random.unit() * (et.angle1 - et.angle2);
                speed = sim.random.unit() * (et.speed1 - et.speed2) + et.speed2;
                break;
            case 9:
            case 10: {
                if ((count & 1) == 0)
                    angle = et.angle2 * 0.5f + float(way / 2) * et.angle2;
                else
                    angle = float((way + 1) / 2) * et.angle2;
                angle = angle + (float(idx) * two_pi) / float(count);
                if (way & 1) angle = angle * -1.0f;
                if (mode == 9) angle = angle + aim;
                angle = et.angle1 + angle;
                break;
            }
            default:
                angle = et.angle1;
                break;
            }
            const float stored = wrap_angle(angle);
            float x = et.spawn[0], y = et.spawn[1], z = et.spawn[2];
            if (et.spin != 0.0f) {
                float ox = 0, oy = 0;
                polar_xy(stored, et.spin, ox, oy);
                x = x + ox;
                y = y + oy;
            }
            sim.sink.bullet_spawn(e.serial, u32(slot), u32(way), u32(idx), x, y, z, stored, speed,
                                  et.sprite, et.subtype);
        }
    }
    return 0;
}

}  // namespace th13
