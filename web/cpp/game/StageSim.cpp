// stage1 模拟核心：资源装载、逐帧积分与线程驱动。锚点与合同见 docs/plans/event-contract.md。
#include "StageSim.hpp"

#include "BulletAppearance.hpp"
#include "StageMath.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace th13 {

namespace {

// 仓库文件读取（raw dump 目录；oracle 侧同源）。
bool read_file(const std::string& path, std::vector<u8>& out) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    out.resize(size_t(size < 0 ? 0 : size));
    const size_t got = out.empty() ? 1 : std::fread(out.data(), 1, out.size(), file);
    std::fclose(file);
    return got == out.size();
}

// ECLI 递归 + ANIM 名单核验（0x4795b0 注册序的行为子集）。
class StageResourceProvider final : public EclResourceProvider {
public:
    std::string resource_dir;
    StageSim* sim = nullptr;
    std::vector<std::string>* anim_list = nullptr;
    bool read(const std::string& name, std::vector<u8>& data) override {
        return read_file(resource_dir + "/" + name, data);
    }
    bool animation(u32, const std::string& name) override {
        // 依赖清单装载核验：ANM 文件必须在 raw 目录中存在（内容解析不在 stage1 范围）。
        std::vector<u8> probe;
        if (!read_file(resource_dir + "/" + name, probe)) {
            if (sim) sim->error_ = "missing ANM dependency: " + name;
            return false;
        }
        if (anim_list) anim_list->push_back(name);
        return true;
    }
};

}  // namespace

void StageEt::reset() {
    // 0x41b260 case 0x61（0x41d573）：槽全清后置默认 {angle1=0, count=1,
    // ways=1, speed1=2.0}；dword[0x235] 位型 0x00010001 = count/ways 各 1。
    sprite = subtype = 0;
    spawn[0] = spawn[1] = spawn[2] = 0;
    angle1 = angle2 = 0;
    speed1 = 2.0f;
    speed2 = 0;
    spin = 0;
    count = 1;
    ways = 1;
    aim = 0;
    offset[0] = offset[1] = offset[2] = 0;
    absolute[0] = absolute[1] = absolute[2] = 0;
    // etEx 队列（0x41df90 写入、600 etNew 全清）：0x41d573 槽清含 ex 区。
    for (auto& item : ex) {
        item.op = 0;
        item.flags = 0;
        for (float& value : item.args) value = 0;
    }
    ex_next = 0;
}

bool StageSim::load(const char* resource_dir, const char* event_path, const char* stage) {
    gvars.sim = this;
    commands.sim = this;
    const std::string stage_file = stage ? stage : "st01.ecl";
    std::vector<u8> st01;
    if (!read_file(std::string(resource_dir) + "/" + stage_file, st01)) {
        error_ = "cannot read " + stage_file;
        return false;
    }
    StageResourceProvider provider;
    provider.resource_dir = resource_dir;
    provider.sim = this;
    provider.anim_list = &anim_list;
    if (program.attach(st01.data(), u32(st01.size()), &provider) != 0 || !program.error.empty()) {
        error_ = program.error.empty() ? "st01.ecl attach failed" : program.error;
        return false;
    }
    if (anim_list.size() < 2) {
        error_ = "st01.ecl ANIM list incomplete";
        return false;
    }
    if (!sink.open(event_path)) {
        error_ = "cannot open event output";
        return false;
    }
    return true;
}

bool StageSim::begin(EclResourceProvider& provider, const char* stage_file, i32 difficulty,
                     const char* event_path) {
    gvars.sim = this;
    commands.sim = this;
    interactive = true;
    difficulty_rank = difficulty;
    diff = 0;  // begin 的入参是掩码（1 << DIFF），取尾零还原 DIFF
    while (difficulty > 1) {
        difficulty >>= 1;
        ++diff;
    }
    std::vector<u8> stage;
    if (!provider.read(stage_file, stage)) {
        error_ = std::string("cannot read ") + stage_file;
        return false;
    }
    if (program.attach(stage.data(), u32(stage.size()), &provider) != 0 || !program.error.empty()) {
        error_ = program.error.empty() ? "stage attach failed" : program.error;
        return false;
    }
    if (anim_list.size() < 2) {
        error_ = "stage ANIM list incomplete";
        return false;
    }
    // 回放对拍模式：事件落盘开启（§7）；浏览器侧保持静默。
    if (event_path && !sink.open(event_path)) {
        error_ = "cannot open event output";
        return false;
    }
    replay_events = event_path != nullptr;
    return true;
}

// 0x4234b0/0x422f80 共用的推进：elapsed+1；到终点写终值并失活（duration 清 0），
// 否则按缓动模式线性插值。特殊模式 7/17/8 在 setup 处拒绝（st01 窗口外）。
namespace {
void step_polar_itp(StagePolarItp& itp, float& out_a, float& out_b) {
    ++itp.elapsed;
    if (itp.elapsed >= i32(itp.duration)) {
        out_a = itp.to_a;
        out_b = itp.to_b;
        itp.duration = 0;
        return;
    }
    const float t = ease_interp(itp.mode, float(itp.elapsed), itp.duration);
    out_a = itp.from_a + t * (itp.to_a - itp.from_a);
    out_b = itp.from_b + t * (itp.to_b - itp.from_b);
}

void step_pos_itp(StagePosItp& itp, float* out) {
    ++itp.elapsed;
    if (itp.elapsed >= i32(itp.duration)) {
        out[0] = itp.target[0];
        out[1] = itp.target[1];
        out[2] = itp.target[2];
        itp.duration = 0;
        return;
    }
    if (itp.mode == 8) {
        // 贝塞尔/厄米特（0x4230d4）：权重 f32 落点后逐对累加（次序与原版一致）。
        const float t = float(itp.elapsed) / float(i32(itp.duration));
        const double x = t;
        const float w_start = static_cast<float>((2 * x + 1) * (x - 1) * (x - 1));
        const float w_target = static_cast<float>(x * x * (3 - 2 * x));
        const float w_p1 = static_cast<float>((1 - x) * (1 - x) * x);
        const float w_p2 = static_cast<float>((x - 1) * x * x);
        for (int i = 0; i < 3; ++i) {
            const float a = itp.start[i] * w_start;
            const float b = itp.target[i] * w_target;
            const float c = itp.p1[i] * w_p1;
            const float d = itp.p2[i] * w_p2;
            out[i] = ((a + b) + c) + d;
        }
        return;
    }
    const float t = ease_interp(itp.mode, float(itp.elapsed), itp.duration);
    out[0] = itp.start[0] + t * (itp.target[0] - itp.start[0]);
    out[1] = itp.start[1] + t * (itp.target[1] - itp.start[1]);
    out[2] = itp.start[2] + t * (itp.target[2] - itp.start[2]);
}

// 0x4736a0：mode0 产极坐标位移写入 aux[0..1]；mode2/3 圆周推进角与半径。
void step_mover_delta(StageMover& m) {
    switch (m.mode & 0xF) {
    case 0:
        polar_xy(m.angle, m.speed * 1.0f, m.aux[0], m.aux[1]);
        break;
    case 2:
    case 3:
        m.radius = m.radius + m.growth * 1.0f;
        m.angle = wrap_angle(m.speed * 1.0f + m.angle);
        break;
    default:
        break;
    }
}

// 0x473780：mode0 子位置 += aux；mode2 子位置 = aux(圆心) + polar(角,半径)；xy 量化 0.01。
void accumulate_mover(StageMover& m) {
    switch (m.mode & 0xF) {
    case 0:
        m.pos[0] = m.pos[0] + m.aux[0];
        m.pos[1] = m.pos[1] + m.aux[1];
        m.pos[2] = m.pos[2] + m.aux[2];
        break;
    case 2:
    case 3: {
        float ox = 0, oy = 0;
        polar_xy(m.angle, m.radius, ox, oy);
        m.pos[0] = m.aux[0] + ox;
        m.pos[1] = m.aux[1] + oy;
        m.pos[2] = m.aux[2] + 0.0f;
        break;
    }
    default:
        break;
    }
    m.pos[0] = round001(m.pos[0]);
    m.pos[1] = round001(m.pos[1]);
}
}  // namespace

void StageSim::integrate(StageEnemy& e) {
    // 0x419450 序：vel 插值 → pos 插值 → movePosTime/极坐标 delta → 累加 → 0x419950。
    // 0x419450:34-44：极坐标插值步进（elapsed 为 float，0x401400 每帧 +1.0）。
    // 圆周模式（mode 2/3）不写回 angle——插值角维（409 setup 的 from=to=当前
    // 角）不得覆写积分累计的 angle，仅 speed 维写回（vm+0xa0）。
    if (e.vel.polar.active()) {
        float a = 0, b = 0;
        step_polar_itp(e.vel.polar, a, b);
        const u32 mode = e.vel.mode & 0xF;
        if (mode != 2 && mode != 3) e.vel.angle = wrap_angle(a);
        e.vel.speed = b;
    }
    if (e.pos.polar.active()) {
        float a = 0, b = 0;
        step_polar_itp(e.pos.polar, a, b);
        const u32 mode = e.pos.mode & 0xF;
        if (mode != 2 && mode != 3) e.pos.angle = wrap_angle(a);
        e.pos.speed = b;
    }
    if (e.vel.postime.active()) {
        float out[3];
        step_pos_itp(e.vel.postime, out);
        // 0x419450:71-79：插值增量写 vel mover 的 aux（vm+0xbc），
        // 由 0x473780 case0 累加（pos += aux），不直接写子位置。
        e.vel.aux[0] = out[0] - e.vel.pos[0];
        e.vel.aux[1] = out[1] - e.vel.pos[1];
        e.vel.aux[2] = out[2] - e.vel.pos[2];
    } else {
        step_mover_delta(e.vel);
    }
    if (e.pos.postime.active()) {
        float out[3];
        step_pos_itp(e.pos.postime, out);
        e.pos.aux[0] = out[0] - e.pos.pos[0];
        e.pos.aux[1] = out[1] - e.pos.pos[1];
        e.pos.aux[2] = out[2] - e.pos.pos[2];
    } else {
        step_mover_delta(e.pos);
    }

    accumulate_mover(e.vel);

    accumulate_mover(e.pos);

    // 0x419950：主位置 += (vel.pos + pos.pos) − 主位置，再量化；活动界限时钳制并回写 vel 子位置。
    for (int i = 0; i < 3; ++i) {
        const float delta = (e.vel.pos[i] + e.pos.pos[i]) - e.position[i];
        e.position[i] = e.position[i] + delta;
    }
    e.position[0] = round001(e.position[0]);
    e.position[1] = round001(e.position[1]);
    if (e.flags & 0x40000u) {
        const float hw = e.limit_half[0] * 0.5f;
        const float hh = e.limit_half[1] * 0.5f;
        if (e.position[0] < e.limit_center[0] - hw)
            e.position[0] = e.limit_center[0] - hw;
        else if (e.position[0] > e.limit_center[0] + hw)
            e.position[0] = e.limit_center[0] + hw;
        if (e.position[1] < e.limit_center[1] - hh)
            e.position[1] = e.limit_center[1] - hh;
        else if (e.position[1] > e.limit_center[1] + hh)
            e.position[1] = e.limit_center[1] + hh;
        e.vel.pos[0] = e.position[0] - e.pos.pos[0];
        e.vel.pos[1] = e.position[1] - e.pos.pos[1];
        e.vel.pos[2] = e.position[2] - e.pos.pos[2];
    }
}

bool StageSim::in_bounds(StageEnemy& e) {
    // 0x419450 尾：+0x3F30/+0x3F34（ANM 派生）无 ANM 恒 0。
    const float hw = 0.0f, hh = 0.0f;
    if (e.position[0] + hw < -192.0f || 192.0f < e.position[0] - hw) {
        if ((e.flags & 0x20000u) && !(e.flags & 0x4u)) return false;
    } else if (e.position[1] + hh < 0.0f || 448.0f < e.position[1] - hh) {
        if ((e.flags & 0x20000u) && !(e.flags & 0x8u)) return false;
    } else {
        e.flags |= 0x20000u;
    }
    return true;
}

StageEnemy* StageSim::find_by_script_id(i32 id) {
    // 0x417f40：id 0 → null；线性查找，未命中返回最后检视者（原版行为），空表 null。
    if (id == 0) return nullptr;
    StageEnemy* last = nullptr;
    for (auto& entry : enemies)
        if (!entry->dead) {
            last = entry.get();
            if (entry->script_id == id) return last;
        }
    return last;
}

StageEnemy* StageSim::boss() const {
    for (i32 slot : boss_slot)
        if (slot >= 0)
            for (auto& entry : enemies)
                if (!entry->dead && entry->script_id == slot) return entry.get();
    return nullptr;
}

StageEnemy* StageSim::spawn_enemy(const char* routine, float x, float y, float life, StageEnemy* parent,
                                  bool inherit_anm, bool mirror) {
    (void)inherit_anm;  // ANM 遗产拷贝（0x41fcc0:73-79 无条件 12 dword）不参与事件，暂略
    auto enemy = std::make_unique<StageEnemy>();
    enemy->serial = next_serial_++;  // mgr+0xBC 序号：root=0，此后逐次 +1
    enemy->script_id = i32(enemy->serial);
    enemy->trace_id = parent ? parent->script_id : 0;
    enemy->routine = routine;
    enemy->life = life;
    enemy->vel.pos[0] = x;
    enemy->vel.pos[1] = y;
    // 0x4183f0:28：param2[6]<<20 无条件写入 flags bit20（在构造趟 :65 之前，
    // 构造趟内 404/405 族按镜像翻转角度）。
    enemy->flags = (enemy->flags & ~0x100000u) | (u32(mirror) << 20);
    enemy->threads = std::make_unique<EclThreads>(program, &gvars, &random);
    enemy->threads->main.commands = &commands;
    const u8 difficulty = parent ? parent->threads->main.difficulty
                                 : u8(interactive ? difficulty_rank : 0x08);  // 契约：Lunatic
    enemy->threads->start(routine, difficulty);
    StageEnemy* raw = enemy.get();
    if (std::strcmp(routine, "Boss") == 0) boss_main_serial = raw->serial;
    // 合同 §3.1：root（parent 为空）不由事件产生；enmCreate 族才发 enemy_spawn。
    // 事件在构造趟之前（0x4183f0 入口即探参观察点）。
    if (parent) sink.enemy_spawn(raw->serial, routine, x, y, 0.0f, life, parent->serial);
    tick_enemy(*raw);  // 0x4183f0:65 构造内直调 0x41a8a0（首趟，计入当前帧）
    raw->tick_gate = true;  // 0x41a8a0:31 干活后置 0x80000；待 MGR_UPDATE 到达时清
    // 0x4183f0:106-124 挂链在构造趟之后——构造趟内 enmCreate 的后代先入链，
    // 宿主随后尾插（vector 序 = 链序）。这决定「创建者是否链尾」，进而决定
    // 新敌机创建帧是否被遍历到达（oracle 探针实证：root 构造趟先建 Maple，
    // 链序 [Maple, root]；root 为尾时其新敌机次帧空转、次次帧才干活）。
    enemies.push_back(std::move(enemy));
    ++spawned_;
    return raw;
}

void StageSim::tick_enemy(StageEnemy& e) {
    if (e.dead || !error_.empty()) return;
    StageEnemy* saved = current;
    current = &e;
    integrate(e);
    if (in_bounds(e)) {
        e.age += 1.0f;
        const int result = e.threads->step(1.0f);
        if (result == -2) {
            const std::string detail = e.threads->error.empty()
                                           ? (error_.empty() ? "unknown ECL error" : error_)
                                           : e.threads->error;
            error_ = "enemy " + std::to_string(e.serial) + " (" + e.routine + ") frame " +
                     std::to_string(frame) + ": " + detail;
        }
        else if (result)
            e.dead = true;  // 主线程结束（0x418e00 销毁支路）
        // 0x41a8a0 calls 0x419a90 AFTER ECL; this is where the independent
        // enemy-age timeout is tested. A timeout arms the new routine for the
        // next tick (unlike the immediate post-hit continuation in damage()).
        if (!e.dead && error_.empty()) interrupt_enemy(e);
        if (e.invulnerable > 0) --e.invulnerable;
    } else {
        e.dead = true;  // 出界自杀（0x419450 返回 -1）
    }
    current = saved;
}

bool StageSim::interrupt_enemy(StageEnemy& e) {
    // 0x4218f0 first examines ONLY the first live health slot, then the first
    // live slot with a positive timeout. MBoss uses slot1 and overrides its
    // timeout with MBossEscape, while health defeat must still reach MBossDead.
    e.phase_life_remaining = i32(e.life);
    e.phase_life_threshold = 0;
    EclContext::Interrupt* fired = nullptr;
    bool timed_out = false;
    for (auto& slot : e.interrupts) {
        if (slot.life < 0) continue;
        e.phase_life_remaining = i32(e.life) - slot.life;
        e.phase_life_threshold = slot.life;
        if (e.life <= float(slot.life)) fired = &slot;
        break;
    }
    if (!fired) {
        for (auto& slot : e.interrupts) {
            if (slot.life < 0 || slot.timeout <= 0) continue;
            if (e.age >= float(slot.timeout)) {
                fired = &slot;
                timed_out = true;
            }
            break;
        }
    }
    if (!fired) return false;
    const std::string label = timed_out ? fired->timeout_label : fired->health_label;
    e.life = float(fired->life);  // discard overkill at phase boundaries
    fired->life = -1;  // consume before running a routine which may re-arm it
    e.age = 0;
    e.phase_timed_out = timed_out;
    e.phase_life_remaining = 0;
    e.phase_life_threshold = i32(e.life);
    const u8 difficulty = e.threads->main.difficulty;
    // Clears async threads and ECL stack/interpolators, not the enemy-owned
    // interrupt table or ANM handles (0x4176c0 + 0x417740 + 0x4797d0).
    if (!e.threads->restart(label.c_str(), difficulty))
        error_ = "enemy interrupt " + label + ": " + e.threads->error;
    return true;
}

bool StageSim::run(u32 frames) {
    if (!error_.empty()) return false;
    if (!start()) return false;
    for (frame = 1; frame < frames && error_.empty(); ++frame) {
        sink.set_frame(frame);
        if (!step_frame()) break;
    }
    sink.close();  // 落盘（读回/比对前保证缓冲清净）
    return error_.empty() && sink.ok();
}

bool StageSim::start() {
    if (!error_.empty()) return false;
    // 第 0 帧 = stage 装载趟：root（serial 0，承载 main）创建并跑构造内首趟。
    frame = 0;
    sink.set_frame(0);
    spawn_enemy("main", 0.0f, 0.0f, 0.0f, nullptr, false);
    return error_.empty();
}

bool StageSim::step_frame() {
    if (!error_.empty()) return false;
    // 敌链清空（root 结束且无存活者）：无头窗口内不会到达（root 常驻），
    // 交互模式可能到达（合成脚本/全灭）——空转保步进可用。
    if (enemies.empty()) {
        if (interactive) advance_bullets();
        return true;
    }
    // 0x418e00:16-30：游标在每个敌机处理前取其实时 next；0x41a8a0 返回
    // -1（死亡）当场摘链——已死敌机不占链位，链尾创建者的新敌机是否被
    // 本帧遍历到达取决于创建时游标位置（vector 插入序 = 链序）。
    u32 i = 0;
    while (true) {
        StageEnemy* entry = enemies[i].get();
        const u32 next = i + 1;
        const bool has_next = next < enemies.size();
        bool remove = entry->dead;
        if (!entry->dead) {
            // 0x41a8a0:30-31：0x80000 位已置则本帧空转；0x418e00:20 返回 0 后清位。
            if (!entry->tick_gate) {
                tick_enemy(*entry);
                entry->tick_gate = true;
            }
            if (entry->dead) remove = true;
            else entry->tick_gate = false;
        }
        if (remove) {
            // 敌机销毁（0x418e00 销毁支路）先经宿主回收其槽位 VM 句柄
            // （enemies_from_ecl 路径的敌机本体 VM 所有权在 registry——
            // 敌机结构消亡后句柄即悬空，此处回收防泄漏）。damage() 致死
            // 的敌机已在其死亡帧同帧回收（0x419a90 当趟销毁语义），此处
            // 只兜出界自杀/525 清场等 dead 置位路径；retire 幂等（句柄
            // 已归零则空转）。
            if (retire_sink) retire_sink(entry->serial);
            enemies.erase(enemies.begin() + i);  // 0x418e00 else：当场摘链（无 RNG）
            if (i >= enemies.size()) break;
            continue;  // 游标走处理前记下的 next（摘除后恰为当前位置）
        }
        if (!error_.empty()) break;
        if (!has_next) break;
        i = next;
    }
    if (interactive) advance_bullets();
    return error_.empty();
}

// ---- etEx 行为队列（0x40f030）：消费到首个阻塞项 ----
// 阻塞三条件（0x40f087-0x40f0ad，汇编核对）：
//   op == 0：队列终止（游标不前进，之后每次调用都在该项停住）；
//   entry.flags == 0 且 bb4 除 0x100 外含任意位：串行项等活跃行为清位；
//   (op & bb4) != 0：该行为位已活跃（同类项不叠发）。
// 每项消费后游标 +1（op 0x8000/0x10000/0x2000/0x80000000 的特例在分支内）。
// 未在 st01 出现的 op（0x80/0x100/0x1000/0x10000/0x20000/0x40000/0x80000/
// 0x200000/0x400000/0x800000/0x1000000）显式报错，不静默跳过。
void StageSim::process_bullet_queue(StageBullet& bullet, bool spawn_time) {
    (void)spawn_time;  // 0x40f030 的两种调用点共用同一消费语义
    while (bullet.ex_cursor < 18) {
        const StageEt::Ex& entry = bullet.ex[bullet.ex_cursor];
        if (entry.op == 0) return;
        if (entry.flags == 0 && (bullet.behavior_flags & 0xfffffeffu) != 0) return;
        if ((entry.op & bullet.behavior_flags) != 0) return;
        const float* args = entry.args;        // +0x00..+0x0C 浮点
        const i32 i4 = i32(entry.args[4]);     // +0x10
        const i32 i5 = i32(entry.args[5]);     // +0x14
        switch (entry.op) {
        case 1:  // 减速突进（0x40f0f3：bb4|=1; 0x4016c0(0) 计时清零; f20=0）
            bullet.behavior_flags |= 1;
            bullet.ex1_timer = 0;
            break;
        case 2:  // 透明占位（default 分支 = 仅推进游标）
            break;
        case 4: {  // steer/accel（0x40f11a：step=args[0]、target=args[1]）
            bullet.steer[0] = 0;
            bullet.steer[1] = 0;
            bullet.steer[2] = 0;
            bullet.ex4_timer = 0;
            const float step = args[0];
            float target = args[1];
            if (target <= -990.0f || target >= 990.0f) {
                // 0x443f60(弹体位置)：出生点→玩家（此处弹体即出生位置）。
                const float dy = player_y - bullet.y, dx = player_x - bullet.x;
                target = (dy == 0.0f && dx == 0.0f)
                             ? 1.5707964f
                             : float(std::atan2(static_cast<double>(dy),
                                                static_cast<double>(dx)));
            }
            bullet.ex4_duration = i4;
            // 0x412510(f60, target, step)：steer = polar(target, step)。
            bullet.steer[0] = std::cos(static_cast<double>(target)) * step;
            bullet.steer[1] = std::sin(static_cast<double>(target)) * step;
            bullet.steer[2] = 0;
            break;
        }
        case 8:  // 螺旋加速（0x40f1da：f94=args[0]、f98=args[1]、fb4=i4）
            bullet.ex8_speed_delta = args[0];
            bullet.ex8_angle_delta = args[1];
            bullet.ex8_duration = i4;
            bullet.ex8_timer = 0;
            break;
        case 0x10: {  // 减停-转向循环（0x40f210：fe0 目标角按 f6 模式解析）
            bullet.behavior_flags |= 0x10;
            const float f0 = args[0];
            switch (i32(entry.args[6])) {  // f6 = +0x18 模式选择
            case 0: {
                float target = f0;
                if (target <= -990.0f || target >= 990.0f) {
                    const float dy = player_y - bullet.y, dx = player_x - bullet.x;
                    target = (dy == 0.0f && dx == 0.0f)
                                 ? 1.5707964f
                                 : float(std::atan2(static_cast<double>(dy),
                                                    static_cast<double>(dx)));
                }
                bullet.ex10_angle = wrap_angle(target);
                break;
            }
            case 1:
            case 4:
                bullet.ex10_angle = f0;
                break;
            case 2: {  // 0x800000 快照位置 → 玩家向（st01 未用 0x800000，快照恒 0）
                const float dy = player_y - bullet.saved_pos[1];
                const float dx = player_x - bullet.saved_pos[0];
                const float base = (dy == 0.0f && dx == 0.0f)
                                       ? 1.5707964f
                                       : float(std::atan2(static_cast<double>(dy),
                                                          static_cast<double>(dx)));
                bullet.ex10_angle =
                    wrap_angle(wrap_angle(base + f0));
                break;
            }
            case 3:
                bullet.ex10_angle = wrap_angle(wrap_angle(bullet.saved_angle + f0));
                break;
            case 5: {
                // 0x472e10 signed_unit：> -990 字面量（>=990 加瞄准角）、
                // <= -990 相对当前角；均 wrap。
                const float unit = random.signed_unit();
                float value;
                if (f0 > -990.0f) {
                    value = f0 < 990.0f ? unit * f0
                                        : unit * f0 +
                                              float(std::atan2(
                                                  static_cast<double>(player_y - bullet.y),
                                                  static_cast<double>(player_x - bullet.x)));
                } else {
                    value = bullet.angle + unit * f0;
                }
                bullet.ex10_angle = wrap_angle(value);
                break;
            }
            case 6:  // 原始 signed_unit×f0，不 wrap（0x40f3f6）
                bullet.ex10_angle = random.signed_unit() * f0;
                break;
            default:
                error_ = "etEx op 0x10 unknown angle mode " + std::to_string(i32(entry.args[6]));
                return;
            }
            // f1 <= -999 → 保持当前速度（0x40f40b 的 fcomi 链）。
            bullet.ex10_speed = args[1] <= -999.0f ? bullet.speed : args[1];
            bullet.ex10_duration = i4;
            bullet.ex10_repeats = i5;
            bullet.ex10_count = 0;
            bullet.ex10_mode = i32(entry.args[6]);
            bullet.ex10_timer = 0;
            bullet.ex10_time = 0;
            break;
        }
        case 0x40: {  // 边界反弹（0x40f41b：1024/102c/1030/1044/1048/1050）
            bullet.behavior_flags |= 0x40;
            bullet.ex40_speed = args[0];
            if ((u32(i5) & 0x20u) == 0) {
                bullet.ex40_w = 448.0f;  // 0x43c00000
                bullet.ex40_h = 448.0f;
            } else {
                bullet.ex40_w = args[1];
                bullet.ex40_h = args[2];
            }
            bullet.ex40_bounces = 0;
            bullet.ex40_limit = i4;
            bullet.ex40_mode = u32(i5);
            break;
        }
        case 0x200: {  // 变弹型（0x40f6a4：1354=i4、1356=i5&0x7fff、弹表重载）
            bullet.sprite = u32(u16(i4));
            bullet.subtype = u32(u16(i5) & 0x7fffu);
            if (const BulletAppearance* appearance = bullet_appearance(i32(bullet.sprite))) {
                bullet.hit_size[0] = appearance->scale;
                bullet.hit_size[1] = appearance->scale;
            } else {
                bullet.hit_size[0] = 4.0f;
                bullet.hit_size[1] = 4.0f;
            }
            // (u32)i5 & 0x8000 → 0x4124d0（出场 ANM 重置）：纯表现，无事件。
            break;
        }
        case 0x400:  // 立即激活（0x411380）：state 1/2 → 3、延迟计数 = 1
            if (bullet.state == 1 || bullet.state == 2) {
                bullet.state = 3;
                bullet.delay = 1;
            }
            break;
        case 0x2000: {
            // 子弹发射（0x40f79a）：下一项为角度/条数描述符，本项与描述符
            // 一并跳过（游标 +2）。参数打包见 i4：byte0=子弹 ex 种子游标、
            // byte1=subtype（有符号字节）、byte2=sprite、byte3&0x7f=aim 模式、
            // bit31=发射后立即激活（0x411380）。
            if (bullet.ex_cursor + 1 >= 18) {
                error_ = "etEx op 0x2000 descriptor outside queue";
                return;
            }
            const StageEt::Ex& descriptor = bullet.ex[bullet.ex_cursor + 1];
            StageEt temp;
            temp.sprite = u32((u32(i4) >> 16) & 0xffu);
            temp.subtype = u32(u8(u32(i4) >> 8));
            temp.angle1 = descriptor.args[0];  // 描述符 f0/f1
            temp.angle2 = descriptor.args[1];
            temp.speed1 = args[0];
            temp.speed2 = args[1];
            temp.count = i16(u32(i5) & 0xffffu);
            temp.ways = i16(u32(descriptor.args[4]) & 0xffffu);
            temp.aim = i16((u32(i4) >> 24) & 0x7fu);
            for (u32 n = 0; n < 18; ++n) temp.ex[n] = bullet.ex[n];  // 队列整块继承
            const float spawn[3] = {bullet.x, bullet.y, bullet.z};
            emit_et(temp, bullet.source_enemy, 0, spawn, 0.0f, u32(i4) & 0xffu);
            bullet.ex_cursor += 1;  // 描述符项（统一尾块再 +1）
            if (i4 < 0) {           // bit31：子发射后父弹立即激活
                if (bullet.state == 1 || bullet.state == 2) {
                    bullet.state = 3;
                    bullet.delay = 1;
                }
            }
            break;
        }
        case 0x4000:  // 透明占位（0x40f030 大 op 链未命中 → 仅推进）
            break;
        case 0x100000: {  // 命中位翻转（0x40fc1b：字面掩码 0xfffffe1f / 0xfffffe3f|0x20）
            if (i4 == 0) {
                bullet.flags &= 0xfffffe1fu;
            } else {
                bullet.flags = (bullet.flags & 0xfffffe3fu) | 0x20u;
            }
            break;
        }
        case 0x80000000:  // 帧等待（0x40fe60：i4<1 直接过；否则 bb4 位 + 倒计时）
            if (i4 < 1) break;
            bullet.behavior_flags |= 0x80000000u;
            bullet.wait_time = float(i4);
            bullet.wait_timer = i4 - 1;
            break;
        default: {
            char hex[12];
            std::snprintf(hex, sizeof hex, "%x", entry.op);
            error_ = std::string("unimplemented etEx op 0x") + hex;
            return;
        }
        }
        if (!error_.empty()) return;
        bullet.ex_cursor += 1;
    }
}

// ---- 弹体每帧行为派发（0x40ddb5..0x40dea7 的位测试链）----
void StageSim::step_bullet_behaviors(StageBullet& bullet) {
    // op 1（0x410230）：计时 <0x11 期间 vel = polar(角, 速度+5-t*0.3125)。
    if (bullet.behavior_flags & 1u) {
        if (bullet.ex1_timer < 0x11) {
            const float bonus = 5.0f - float(bullet.ex1_timer) * 5.0f * 0.0625f;
            bullet.vel[0] = std::cos(static_cast<double>(bullet.angle)) * (bonus + bullet.speed);
            bullet.vel[1] = std::sin(static_cast<double>(bullet.angle)) * (bonus + bullet.speed);
            bullet.vel[2] = 0;
            bullet.ex1_timer += 1;
        } else {
            bullet.behavior_flags &= ~1u;
        }
    }
    // op 4（0x4102b0）：timer < duration 时 vel += steer；|vel|>1e-4 重算角/速。
    if (bullet.behavior_flags & 4u) {
        if (bullet.ex4_timer < bullet.ex4_duration) {
            bullet.ex4_timer += 1;
            bullet.vel[0] += bullet.steer[0];
            bullet.vel[1] += bullet.steer[1];
            bullet.vel[2] += bullet.steer[2];
            const float ax = std::abs(bullet.vel[0]);
            const float ay = std::abs(bullet.vel[1]);
            if (ax > 0.0001f || ay > 0.0001f) {
                // 0x4977aa = atan2(y, x)（x87 双精度）；0x472f30 归一。
                bullet.angle =
                    wrap_angle(wrap_angle(float(std::atan2(static_cast<double>(bullet.vel[1]),
                                                            static_cast<double>(bullet.vel[0])))));
                bullet.speed = std::sqrt(static_cast<double>(bullet.vel[0]) * bullet.vel[0] +
                                         static_cast<double>(bullet.vel[1]) * bullet.vel[1]);
            }
        } else {
            bullet.behavior_flags &= ~4u;
        }
    }
    // op 8（0x410520）：timer < duration 时 angle += delta、speed += delta、
    // vel = polar（0x412510 直写覆盖）。
    if (bullet.behavior_flags & 8u) {
        if (bullet.ex8_timer < bullet.ex8_duration) {
            bullet.ex8_timer += 1;
            bullet.angle = wrap_angle(wrap_angle(bullet.angle + bullet.ex8_angle_delta));
            bullet.speed = bullet.speed + bullet.ex8_speed_delta;
            bullet.vel[0] = std::cos(static_cast<double>(bullet.angle)) * bullet.speed;
            bullet.vel[1] = std::sin(static_cast<double>(bullet.angle)) * bullet.speed;
        } else {
            bullet.behavior_flags &= ~8u;
        }
    }
    // op 0x10（0x4105d0）：减停 → 按模式转向 → 重复 repeats 次。
    if (bullet.behavior_flags & 0x10u) {
        if (bullet.ex10_timer < bullet.ex10_duration) {
            // 减速相：speed × (1 - t/duration)（float 计时）。
            const float decayed =
                bullet.speed - bullet.ex10_time * bullet.speed / float(bullet.ex10_duration);
            bullet.vel[0] = std::cos(static_cast<double>(bullet.angle)) * decayed;
            bullet.vel[1] = std::sin(static_cast<double>(bullet.angle)) * decayed;
            bullet.ex10_timer += 1;
            bullet.ex10_time += 1.0f;
        } else {
            bullet.ex10_count += 1;
            float target = bullet.ex10_angle;
            switch (bullet.ex10_mode) {
            case 0:
            case 5:
                target = bullet.ex10_angle + bullet.angle;
                break;
            case 1:
            case 6: {
                const float dy = player_y - bullet.y, dx = player_x - bullet.x;
                target = (dy == 0.0f && dx == 0.0f)
                             ? 1.5707964f + bullet.ex10_angle
                             : float(std::atan2(static_cast<double>(dy),
                                                static_cast<double>(dx))) +
                                   bullet.ex10_angle;
                break;
            }
            case 2: {
                const float dy = player_y - bullet.saved_pos[1];
                const float dx = player_x - bullet.saved_pos[0];
                target = (dy == 0.0f && dx == 0.0f)
                             ? 1.5707964f + bullet.ex10_angle
                             : float(std::atan2(static_cast<double>(dy),
                                                static_cast<double>(dx))) +
                                   bullet.ex10_angle;
                break;
            }
            case 3:
                target = bullet.saved_angle + bullet.ex10_angle;
                break;
            case 4:
                target = bullet.ex10_angle;
                break;
            default:
                break;  // 0x410616 default：跳过转向，仅恢复速度
            }
            bullet.angle = wrap_angle(target);
            // 0x4106xx：转向帧把速度域直接写为恢复速度并复位计时；未达重复
            // 上限时落 LAB_0041073b 立即推进一次（计时 = 1）。
            bullet.ex10_timer = 0;
            bullet.ex10_time = 0;
            bullet.speed = bullet.ex10_speed;
            bullet.vel[0] = std::cos(static_cast<double>(bullet.angle)) * bullet.ex10_speed;
            bullet.vel[1] = std::sin(static_cast<double>(bullet.angle)) * bullet.ex10_speed;
            if (bullet.ex10_repeats <= bullet.ex10_count) {
                bullet.behavior_flags &= ~0x10u;
            } else {
                bullet.ex10_timer = 1;
                bullet.ex10_time = 1.0f;
            }
        }
    }
    // op 0x40（0x410a80）：边界外按 1050 位反弹，计满 limit 清位。
    if (bullet.behavior_flags & 0x40u) {
        // 0x412400：x ∈ [-w/2, w/2]、y ∈ [224-h/2, 224+h/2] 之外 = 出界。
        const bool outside = bullet.x < -bullet.ex40_w * 0.5f ||
                             bullet.x > bullet.ex40_w * 0.5f ||
                             bullet.y < 224.0f - bullet.ex40_h * 0.5f ||
                             bullet.y > 224.0f + bullet.ex40_h * 0.5f;
        if (outside) {
            bool bounced = false;
            const bool reflect = (bullet.ex40_mode & 0x10u) == 0;  // bit10 = 只计数
            if ((bullet.ex40_mode & 1u) != 0 &&
                bullet.y < 224.0f - bullet.ex40_h * 0.5f) {  // 顶（0x410960）
                bounced = true;
                if (reflect) {
                    bullet.angle = wrap_angle(-bullet.angle);
                    bullet.y = (448.0f - bullet.ex40_h) - bullet.y;
                }
            }
            if ((bullet.ex40_mode & 2u) != 0 &&
                bullet.y > 224.0f + bullet.ex40_h * 0.5f) {  // 底（0x4109f0）
                bounced = true;
                if (reflect) {
                    bullet.angle = wrap_angle(-bullet.angle);
                    bullet.y = (bullet.ex40_h + 448.0f) - bullet.y;
                }
            }
            if ((bullet.ex40_mode & 8u) != 0 &&
                bullet.x > bullet.ex40_w * 0.5f) {  // 右（0x4108b0）
                bounced = true;
                if (reflect) {
                    bullet.angle = wrap_angle(wrap_angle(-bullet.angle - 3.1415927f));
                    bullet.x = bullet.ex40_w - bullet.x;
                }
            }
            if ((bullet.ex40_mode & 4u) != 0 &&
                bullet.x < -bullet.ex40_w * 0.5f) {  // 左（0x4107f0）
                bounced = true;
                if (reflect) {
                    bullet.angle = wrap_angle(wrap_angle(-bullet.angle - 3.1415927f));
                    bullet.x = -bullet.ex40_w - bullet.x;
                }
            }
            if (-990.0f < bullet.ex40_speed) bullet.speed = bullet.ex40_speed;
            bullet.vel[0] = std::cos(static_cast<double>(bullet.angle)) * bullet.speed;
            bullet.vel[1] = std::sin(static_cast<double>(bullet.angle)) * bullet.speed;
            if (bounced) {
                bullet.ex40_bounces += 1;
                if (bullet.ex40_limit <= bullet.ex40_bounces)
                    bullet.behavior_flags &= ~0x40u;
            }
        }
    }
    // op 0x80000000 等待（0x40de74 内联）：float 计时 >0 时倒数，归零清位。
    if (bullet.behavior_flags & 0x80000000u) {
        if (bullet.wait_time > 0.0f) {
            bullet.wait_time -= 1.0f;
            bullet.wait_timer = i32(bullet.wait_time);
        } else {
            bullet.behavior_flags &= ~0x80000000u;
        }
    }
}

void StageSim::advance_bullets() {
    // 0x40db10 每弹一帧（队列在管理器链序 = 新弹在头的插入序；行为不抽
    // RNG，序不影响同步）：
    //   state 2（种子 op==2 的延迟弹）：pos += vel×0.5（0x40dbf9 分支）；
    //   随后与普通弹同路（0x40dd8d 门 0x4d4 默认 1.0f 恒过）。
    //   state 1：0x40f030 队列消费 → 行为派发 → pos += vel（0x40dec1）。
    //   出界回收：游戏区外扩（保持既有 ±256/-64..512 缓冲带）。
    // 帧首快照弹数：本帧中途 0x2000 新产弹不入当帧更新（原版头插在游标
    // 之后再到达）。逐弹以本地副本推进再写回：队列内 0x2000 的递归
    // push_back 会使 vector 重分配，禁止持有跨调用引用。
    const size_t count = bullets.size();
    for (size_t i = 0; i < count; ++i) {
        StageBullet bullet = bullets[i];
        if (!bullet.alive) continue;
        if (bullet.state == 2) {
            bullet.x += bullet.vel[0] * 0.5f;
            bullet.y += bullet.vel[1] * 0.5f;
            bullet.z += bullet.vel[2] * 0.5f;
            bullet.state = 1;
        }
        if (bullet.state == 1 || bullet.state == 3) {
            process_bullet_queue(bullet, false);
            if (!error_.empty()) return;
            step_bullet_behaviors(bullet);
        }
        bullet.x += bullet.vel[0];
        bullet.y += bullet.vel[1];
        bullet.z += bullet.vel[2];
        if (bullet.x < -256.0f || bullet.x > 256.0f || bullet.y < -64.0f || bullet.y > 512.0f)
            bullet.alive = false;
        bullets[i] = bullet;
    }
    // 尾部压缩：存活弹前移（稳定序，逐帧 O(n)）。
    size_t keep = 0;
    for (size_t i = 0; i < bullets.size(); ++i) {
        if (bullets[i].alive) bullets[keep++] = bullets[i];
    }
    bullets.resize(keep);
}

// ---- ECL 掉落表执行（0x417ad0 + 0x417b10）----
// RNG 全归属本函数（= 原版 0x417ad0/0x417b10 体内）：
//   主道具（drop_main != 0）：1 个 kind 号道具，无 RNG；
//   散落：初始 signed_unit×1（表空也抽）；每件 unit×1（散布份额
//   0.5..1）→（kind≥10 的 0x438ec0 体内再抽 signed_unit+unit 各 1，
//   由道具宿主 spawn_value_item 承接）→ signed_unit×1（角步进）。
// 道具实体的生成经 StageItemHost 即时同步调用；宿主为空时仅消耗等量
// RNG（原版道具池满/缺席的同构行为），次序不变。
void StageSim::drop_table(StageEnemy& enemy) {
    if (items && enemy.drop_main != 0)
        items->spawn_item(enemy.position[0], enemy.position[1], enemy.position[2],
                          enemy.drop_main, -1.5707964f, 2.2f);
    float angle = random.signed_unit() * 3.1415927f;
    for (int kind = 0; kind < 14; ++kind) {
        for (int n = 0; n < enemy.drop_extra[kind]; ++n) {
            // 0x424770：offset = (cos(angle)×area_x, sin(angle)×area_y)。
            const float ox = std::cos(static_cast<double>(angle)) * enemy.drop_area[0];
            const float oy = std::sin(static_cast<double>(angle)) * enemy.drop_area[1];
            const float jitter = 0.5f + random.unit() * 0.5f;
            const float x = enemy.position[0] + jitter * ox;
            const float y = enemy.position[1] + jitter * oy;
            if (items) {
                if (kind + 1 >= 10)
                    items->spawn_value_item(kind + 1 - 10, x, y, enemy.position[2], 0.5f);
                else
                    items->spawn_item(x, y, enemy.position[2], kind + 1, -1.5707964f, 2.2f);
            }
            angle = wrap_angle(wrap_angle(
                angle + 1.5707964f + random.signed_unit() * 3.1415927f * 0.25f));
        }
    }
    for (i32& slot : enemy.drop_extra) slot = 0;  // memset(EDI+4, 0, 0x3c)
    enemy.drop_main = 0;                          // *param_1 = 0（0x417aff）
}

// ---- 615/616 半径消弹（0x4114f0）----
void StageSim::cancel_bullets_radius(const StageEnemy& enemy, float radius, bool spawn_cancel) {
    for (auto& bullet : bullets) {
        if (!bullet.alive) continue;
        const float dx = bullet.x - enemy.position[0];
        const float dy = bullet.y - enemy.position[1];
        const float limit = radius + bullet.hit_size[0] * 0.5f;
        if (dx * dx + dy * dy > limit * limit) continue;
        bullet.x += bullet.vel[0] * 0.5f;  // 摘除前半帧位移（0x4114f0 尾）
        bullet.y += bullet.vel[1] * 0.5f;
        bullet.z += bullet.vel[2] * 0.5f;
        // 场内判定（0x4114f0 的 ±32 窗）：615 给场内弹产取消道具（kind 9）。
        if (items && spawn_cancel && bullet.x + 32.0f >= -192.0f &&
            bullet.x - 32.0f <= 192.0f && bullet.y + 32.0f >= 0.0f &&
            bullet.y - 32.0f < 448.0f)
            items->spawn_item(bullet.x, bullet.y, bullet.z, 9, -1.5707964f, 2.2f);
        bullet.alive = false;  // state → 4
    }
    if (items && !spawn_cancel)
        items->on_et_clear(enemy.position[0], enemy.position[1], enemy.position[2]);
}

// ---- 发射内核（0x40ffe0 外层 ways/count 循环 + 0x40e840 单弹出生）----
void StageSim::emit_et(const StageEt& et, u32 enemy_serial, u32 slot, const float spawn[3],
                       float aim, u32 ex_seed) {
    // 0x40e840 开头的管理器 +0x60 出生保护（526 etProtectRange）：st01 未用
    // （半径恒 0），半径非零的分支待 526 实装时补。
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
                        random.unit() * static_cast<double>(et.angle1 - et.angle2);
                break;
            case 7:
                speed = static_cast<float>(random.unit() *
                                               static_cast<double>(et.speed1 - et.speed2) +
                                           et.speed2);
                angle = et.angle2 * static_cast<double>(way) + et.angle1 +
                        static_cast<double>(idx) * kTwoPiF / static_cast<double>(count);
                break;
            case 8:
                angle = et.angle2 +
                        random.unit() * static_cast<double>(et.angle1 - et.angle2);
                speed = static_cast<float>(random.unit() *
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
            float x = spawn[0], y = spawn[1];
            // z：0x40e840 写入 spawn z 后无条件覆写 0x3dcccccd（渲染层常量），
            // 事件口径恒 0.1f；spin（et[9]）只作用于 x/y（0x40e840 尾段）。
            const float z = 0.1f;
            if (et.spin != 0.0f) {
                float ox = 0, oy = 0;
                polar_xy(stored, et.spin, ox, oy);
                x = x + ox;
                y = y + oy;
            }
            sink.bullet_spawn(enemy_serial, slot, u32(way), u32(idx), x, y, z, stored, speed,
                              et.sprite, et.subtype);
            if (interactive) {
                // 交互模式同源产弹体实体；无 RNG（事件口径无涉）。
                StageBullet bullet;
                bullet.x = x;
                bullet.y = y;
                bullet.z = z;
                bullet.angle = stored;
                bullet.speed = speed;
                // 速度向量（0x40e840 的 0x412510(b7c, speed, angle) 出生写）。
                bullet.vel[0] = std::cos(static_cast<double>(stored)) * speed;
                bullet.vel[1] = std::sin(static_cast<double>(stored)) * speed;
                bullet.vel[2] = 0;
                bullet.sprite = et.sprite;
                bullet.subtype = et.subtype;
                bullet.source_enemy = enemy_serial;
                // 0x40e840:237-238：弹体尺寸（两轴同值）= 弹型表 +0x104。
                // 表覆盖 0..37；越界类型原版读表外（无合法用例），此处保留
                // StageBullet 默认定标值，待显式报错任务收敛。
                if (const BulletAppearance* appearance = bullet_appearance(i32(et.sprite))) {
                    bullet.hit_size[0] = appearance->scale;
                    bullet.hit_size[1] = appearance->scale;
                }
                // etEx 队列整块拷贝（0x40e840 尾 0xc6 int）+ 种子游标。
                for (u32 n = 0; n < 18; ++n) bullet.ex[n] = et.ex[n];
                bullet.ex_cursor = i32(ex_seed & 0xffu);
                if (bullet.ex_cursor > 18) bullet.ex_cursor = 18;
                // 种子项 op==2 → 延迟弹（0x40e840 尾分支）：state 2、游标越过
                // 种子项、出生位回退 vel×4（此后首帧 pos += vel×0.5 折返）。
                if (bullet.ex_cursor < 18 && bullet.ex[bullet.ex_cursor].op == 2) {
                    const i32 packed = i32(bullet.ex[bullet.ex_cursor].args[4]);
                    bullet.state = 2;
                    bullet.ex_cursor += 1;
                    bullet.x -= bullet.vel[0] * 4.0f;
                    bullet.y -= bullet.vel[1] * 4.0f;
                    bullet.z -= bullet.vel[2] * 4.0f;
                    if (i16(packed) != 1) bullet.delay = packed + 7;  // 出场 ANM 帧数（表现）
                }
                // 出生时队列消费（0x40e840 尾 FUN_0040f030）——op 0x10 mode 5/6
                // 的 RNG 与 0x2000 子发射都在此按项发生。消费先于入列：本地
                // 实体吸收 0x2000 的递归 push（vector 重分配不悬空引用）；
                // 事件序 = 父弹先于子弹（0x40e840 调用序），列表序 = 子弹在
                // 父弹前（原版头插语义）。
                process_bullet_queue(bullet, true);
                if (!error_.empty()) return;
                bullets.push_back(bullet);
            }
        }
    }
}

void StageSim::damage(StageEnemy& enemy, float amount) {
    if (enemy.dead || amount <= 0 || (enemy.flags & 0x21u) ||
        enemy.invulnerable > 0 || (dialog_host && dialog_host->dialog_active())) return;
    if (enemy.spell_active) {
        // 0x4179e0: spells use a 7x integer accumulator, with signed integer
        // division, not a floating 1/7 damage multiplier.
        enemy.spell_life -= i32(amount);
        enemy.life = float((enemy.spell_life - enemy.spell_base_life * 7) / 7 +
                           enemy.spell_base_life);
    } else {
        enemy.life -= amount;
    }
    if (interrupt_enemy(enemy)) {
        // Health interruption inside 0x419a90 immediately executes the new
        // phase. It must not destroy the Boss entity or clear bossWait's slot.
        StageEnemy* saved = current;
        current = &enemy;
        const int result = enemy.threads->step(1.0f);
        if (result == -2) error_ = enemy.threads->error;
        else if (result) enemy.dead = true;
        current = saved;
        if (!enemy.dead) return;
    }
    if (enemy.dead || (enemy.life <= 0.0f && !(enemy.flags & 0x80u))) {
        // 与出界自杀同路：下帧遍历摘链。Boss 槽指向一并清除
        // （oracle 侧同式写 mgr+0x5c[slot]=0，§7.2）。
        for (i32& slot : boss_slot)
            if (slot == enemy.script_id) slot = -1;
        enemy.dead = true;
        // 同帧 VM 回收（0x419a90 命中致死 → 同趟销毁的 ANM 面）。原版命中
        // 检测在敌机自身更新趟内（0x41a8a0 → 0x419a90 → 返回 -1 → 0x418e00
        // 销毁支路），VM 在绘制前已释放——无输入窗出界自杀同路同拍
        // （f528 首例，oracle 实测批内 quad 同帧消失）；移植侧自机弹命中在
        // step_frame 走链之后结算（StageBattle::collide_shots），死亡置位
        // 当帧即回收才能与原版「销毁当帧出捕获面」对齐——回放窗 f124
        // op[16] count 6≠4 的实测根因（修复后 f124-134 逐字节对齐）。
        // vector 摘链仍在下帧游标（[-9946] ENM_CNT_REAL 读 vector 大小的
        // 契约语义不变）；retire 幂等（句柄已归零则空转）。
        if (retire_sink) retire_sink(enemy.serial);
        if (replay_events)  // §7.3：kill 事件（回放会话）
            sink.kill(enemy.serial, enemy.position[0], enemy.position[1]);
    }
}

}  // namespace th13
