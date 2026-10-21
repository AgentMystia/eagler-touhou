// stage1 模拟核心：资源装载、逐帧积分与线程驱动。锚点与合同见 docs/plans/event-contract.md。
#include "StageSim.hpp"

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
    // 0x41d573（case 0x61）：槽全清后置默认 {angle1=0, count=0, ways=1, speed1=2.0}。
    sprite = subtype = 0;
    spawn[0] = spawn[1] = spawn[2] = 0;
    angle1 = angle2 = 0;
    speed1 = 2.0f;
    speed2 = 0;
    spin = 0;
    count = 0;
    ways = 1;
    aim = 0;
    offset[0] = offset[1] = offset[2] = 0;
    absolute[0] = absolute[1] = absolute[2] = 0;
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
    } else {
        e.dead = true;  // 出界自杀（0x419450 返回 -1）
    }
    current = saved;
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

void StageSim::advance_bullets() {
    // 直进弹：出生角恒定（原版弹体的加速/自旋/反射分期接入；实体仅交互模式）。
    for (auto& bullet : bullets) {
        if (!bullet.alive) continue;
        bullet.x += bullet.speed * std::cos(bullet.angle);
        bullet.y += bullet.speed * std::sin(bullet.angle);
        // 出界回收：游戏区外扩 64px（弹幕常规缓冲带）。
        if (bullet.x < -256.0f || bullet.x > 256.0f || bullet.y < -64.0f || bullet.y > 512.0f)
            bullet.alive = false;
    }
    // 尾部压缩：存活弹前移（稳定序，逐帧 O(n)）。
    size_t keep = 0;
    for (size_t i = 0; i < bullets.size(); ++i) {
        if (bullets[i].alive) bullets[keep++] = bullets[i];
    }
    bullets.resize(keep);
}

void StageSim::damage(StageEnemy& enemy, float amount) {
    if (enemy.dead || amount <= 0) return;
    enemy.life -= amount;
    if (enemy.life <= 0.0f) {
        // 与出界自杀同路：下帧遍历摘链。Boss 槽指向一并清除
        // （oracle 侧同式写 mgr+0x5c[slot]=0，§7.2）。
        for (i32& slot : boss_slot)
            if (slot == enemy.script_id) slot = -1;
        enemy.dead = true;
        if (replay_events)  // §7.3：kill 事件（回放会话）
            sink.kill(enemy.serial, enemy.position[0], enemy.position[1]);
    }
}

}  // namespace th13
