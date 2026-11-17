#include "Stage.hpp"

#include <cmath>
#include <cstring>

#include "Motion.hpp"   // wrap_angle / sin_f / cos_f（0x402d90 / 0x405510 / 0x4054f0）

namespace th16 {

namespace {

constexpr float kPi = 3.1415927f;

StageFog fog_from_color(u32 color, float near_, float far_) {
    StageFog f;
    f.color = color;
    f.near_ = near_;
    f.far_ = far_;
    for (int i = 0; i < 4; ++i) f.c[i] = float((color >> (8 * i)) & 0xffu);
    return f;
}

// 0x40d370：七元组相加，颜色字节由浮点截断重建
StageFog fog_add(const StageFog& a, const StageFog& b) {
    StageFog r;
    r.near_ = a.near_ + b.near_;
    r.far_ = a.far_ + b.far_;
    u32 packed = 0;
    for (int i = 0; i < 4; ++i) {
        r.c[i] = a.c[i] + b.c[i];
        packed |= u32(u8(i32(r.c[i]))) << (8 * i);
    }
    r.color = packed;
    return r;
}

StageFog fog_scale(const StageFog& a, float k) {
    StageFog r;
    r.near_ = a.near_ * k;
    r.far_ = a.far_ * k;
    for (int i = 0; i < 4; ++i) r.c[i] = a.c[i] * k;
    return r;
}

// 原版 0x5fc 结构体 memset 0 的纯数据区（链表结点等宿主指针保持默认）
void zero_vm(AnmVm& vm) {
    std::memset(static_cast<void*>(&vm), 0, 0x568);
}

}  // namespace

bool StageManager::load(const u8* bytes, u32 size, i32 stage) {
    stage_number = stage;
    if (!std.parse(bytes, size)) { error = std.error; return false; }
    // 相机先取全局 0x4c168c（0x43cb10：fov π/6，视口 (116,4,408,472)），再由 0x4097c0 覆盖：
    // pos=(0,0,-600) facing=(0,300,600) up=(0,1,0)，pos_offset / facing_offset 清零
    camera = StageCamera{};
    camera.pos = {0, 0, -600.0f};
    camera.facing = {0, 300.0f, 600.0f};
    camera.up = {0, 1.0f, 0};
    distortion_radius_sq = 9610000.0f;  // +0x331c = 0x4b12a310（3100²）
    flags = 1;
    timer.reset();
    ins_index = 0;
    rocking_mode = 0;
    rocking_timer = Timer{};
    facing_interp = {};
    pos_interp = {};
    up_interp = {};
    fog_interp = {};
    frame = 0;
    drawn_instances = culled_instances = drawn_quads = 0;
    // 0x409670 先按 0x4093f0 构造 8 个层精灵 VM（sprite/ins_offset = -1），随后 _memset(this, 0, 0x66f0) 又把它们
    // 整体清零；0x40ac30 的面 VM 表也是 malloc + memset 0。于是未被 ins 14 指派的层精灵 VM 处于全零状态：
    // file_slot 0（text.anm）、script 0、ins_offset 0——0x409e50 每帧照样 0x45f980 推进它们，这是原版可观测行为。
    face_vms.assign(size_t(std.face_vm_count), AnmVm{});
    for (AnmVm& vm : face_vms) zero_vm(vm);
    for (AnmVm& vm : layer_vms) { vm = AnmVm{}; zero_vm(vm); }
    for (auto& id : layer_ids) id = 0;
    return true;
}

void StageManager::start(AnmManager& manager) {
    // 0x40add0：对象激活位 = 1；按遍历顺序分配面 VM
    i32 k = 0;
    for (StdObject& obj : std.objects) {
        obj.flags = 1;
        for (StdQuad& q : obj.quads) {
            if (k >= i32(face_vms.size())) { error = "std: more quads than face vms"; return; }
            AnmVm& vm = face_vms[size_t(k)];
            manager.init_from_template(*anm, vm, q.anm_script);
            vm.parent = nullptr;
            vm.root = nullptr;
            manager.run(vm);
            q.vm_index = i16(k);
            ++k;
        }
    }
    ins_index = 0;
}

Vec3 StageManager::eye() const {
    return {camera.pos.x + camera.pos_offset.x, camera.pos.y + camera.pos_offset.y, camera.pos.z + camera.pos_offset.z};
}

Vec3 StageManager::at() const {
    const Vec3 e = eye();
    return {e.x + camera.facing_norm.x, e.y + camera.facing_norm.y, e.z + camera.facing_norm.z};
}

void StageManager::step_faces(AnmManager& manager) {
    // 0x40aed0：只推进 flags bit0 的对象；全部面 VM 脚本停止（ins_offset < 0）后清 bit0
    for (StdObject& obj : std.objects) {
        if ((obj.flags & 1) == 0) continue;
        i32 alive = 0;
        for (const StdQuad& q : obj.quads) {
            AnmVm& vm = face_vms[size_t(q.vm_index)];
            manager.run(vm);
            if (vm.ins_offset >= 0) ++alive;
        }
        if (alive == 0) obj.flags &= u8(~1u);
    }
}

void StageManager::set_fog_time(i32 time, i32 mode, const StageFog& goal) {
    // 0x409490
    fog_interp.end_time = time;
    fog_interp.initial = camera.fog;
    fog_interp.mode = mode;
    fog_interp.goal = goal;
    fog_interp.timer.reset();
}

StageFog StageManager::step_fog(float game_speed) {
    // 0x40cd10
    Interp<StageFog>& it = fog_interp;
    const i32 end = it.end_time;
    bool finished = end == 0;
    if (end > 0) {
        it.timer.tick(game_speed);
        if (end <= it.timer.current) {
            it.timer.set(end);
            it.end_time = 0;
            finished = true;
        }
    }
    if (finished) return (it.mode != 7 && it.mode != 0x11) ? it.goal : it.initial;
    if (it.mode == 7) {
        it.initial = fog_add(it.initial, it.goal);
        it.current = it.initial;
    } else if (it.mode == 0x11) {
        it.initial = fog_add(it.initial, it.bezier1);
        it.bezier1 = fog_add(it.bezier1, it.goal);
        it.current = it.initial;
    } else if (it.mode == 8) {
        const float r = it.timer.current_f / float(end);
        const float s = r - 1.0f;
        const float h11 = s * r * r;
        const float h00 = s * s * (r + r + 1.0f);
        const float h10 = (1.0f - r) * (1.0f - r) * r;
        const float h01 = r * r * (3.0f - (r + r));
        const StageFog a = fog_add(fog_scale(it.initial, h00), fog_scale(it.goal, h01));
        const StageFog b = fog_add(fog_scale(it.bezier1, h10), fog_scale(it.bezier2, h11));
        it.current = fog_add(a, b);
    } else {
        const float f = interp_ease(it.mode, it.timer.current_f, float(end));
        StageFog delta;
        delta.near_ = (it.goal.near_ - it.initial.near_) * f;
        delta.far_ = (it.goal.far_ - it.initial.far_) * f;
        for (int i = 0; i < 4; ++i) delta.c[i] = (it.goal.c[i] - it.initial.c[i]) * f;
        it.current = fog_add(delta, it.initial);
    }
    return it.current;
}

void StageManager::run_script(AnmManager& manager, float game_speed) {
    // 0x40b3b0 前半：执行 time <= timer.current 的指令
    bool stopped = false;
    while (ins_index >= 0 && ins_index < i32(std.script.size())) {
        const StdInstruction& ins = std.script[size_t(ins_index)];
        if (ins.time > timer.current) break;
        bool jumped = false;
        switch (ins.opcode) {
        case 0:
            stopped = true;
            break;
        case 1: {
            const i32 t = ins.arg_i(1);
            if ((timer.flags & 1) == 0) { timer.current = 0; timer.previous = -999999; timer.current_f = 0; timer.speed_slot = 0; timer.flags |= 1; }
            timer.current = t;
            timer.previous = t - 1;
            timer.current_f = float(t);
            ins_index = std.instruction_at(u32(ins.arg_i(0)));
            jumped = true;
            break;
        }
        case 2: {
            const Vec3 old = camera.pos;
            camera.pos = {ins.arg_f(0), ins.arg_f(1), ins.arg_f(2)};
            camera.pos_delta = {camera.pos.x - old.x, camera.pos.y - old.y, camera.pos.z - old.z};
            break;
        }
        case 3:
            pos_interp.end_time = ins.arg_i(0);
            pos_interp.mode = ins.arg_i(1);
            pos_interp.initial = camera.pos;
            pos_interp.goal = {ins.arg_f(2), ins.arg_f(3), ins.arg_f(4)};
            pos_interp.timer.reset();
            break;
        case 4:
            camera.facing = {ins.arg_f(0), ins.arg_f(1), ins.arg_f(2)};
            break;
        case 5:
            facing_interp.end_time = ins.arg_i(0);
            facing_interp.mode = ins.arg_i(1);
            facing_interp.initial = camera.facing;
            facing_interp.goal = {ins.arg_f(2), ins.arg_f(3), ins.arg_f(4)};
            facing_interp.timer.reset();
            break;
        case 6:
            camera.up = {ins.arg_f(0), ins.arg_f(1), ins.arg_f(2)};
            break;
        case 7:
            camera.fov = ins.arg_f(0);
            break;
        case 8:
            camera.fog = fog_from_color(u32(ins.arg_i(0)), ins.arg_f(1), ins.arg_f(2));
            break;
        case 9:
            set_fog_time(ins.arg_i(0), ins.arg_i(1), fog_from_color(u32(ins.arg_i(2)), ins.arg_f(3), ins.arg_f(4)));
            break;
        case 10:
            pos_interp.end_time = ins.arg_i(0);
            pos_interp.initial = camera.pos;
            pos_interp.bezier1 = {ins.arg_f(2), ins.arg_f(3), ins.arg_f(4)};
            pos_interp.goal = {ins.arg_f(5), ins.arg_f(6), ins.arg_f(7)};
            pos_interp.bezier2 = {ins.arg_f(8), ins.arg_f(9), ins.arg_f(10)};
            pos_interp.mode = 8;
            pos_interp.timer.reset();
            break;
        case 11:
            facing_interp.end_time = ins.arg_i(0);
            facing_interp.initial = camera.facing;
            facing_interp.bezier1 = {ins.arg_f(2), ins.arg_f(3), ins.arg_f(4)};
            facing_interp.goal = {ins.arg_f(5), ins.arg_f(6), ins.arg_f(7)};
            facing_interp.bezier2 = {ins.arg_f(8), ins.arg_f(9), ins.arg_f(10)};
            facing_interp.mode = 8;
            facing_interp.timer.reset();
            break;
        case 12:
            rocking_mode = i32(i8(ins.arg_i(0)));
            if (rocking_mode == 0) camera.pos_offset = {0, 0, 0};
            rocking_timer.reset();
            if (rocking_mode == 2) rocking_timer.set(0x200);
            break;
        case 13:
            bg_color = u32(ins.arg_i(0));
            break;
        case 14: {
            const i32 idx = ins.arg_i(0);
            const i32 script = ins.arg_i(1);
            if (idx < 0 || idx >= kLayerSprites) break;
            AnmVm& vm = layer_vms[idx];
            if (script < 0) {
                if (script == -1) vm.ins_offset = -1;
                if (script == -1 || script == -2) vm.flags_lo &= ~AnmVm::kVisible;
            } else if (anm != nullptr) {
                manager.init_from_template(*anm, vm, script);
                vm.parent = nullptr;
                vm.root = nullptr;
                manager.run(vm);
            }
            layer_ids[idx] = ins.arg_i(2);
            break;
        }
        case 17:
            // 原版：释放旧畸变特效对象，重置其计时/参数并按 arg0（1 → 效果 7，否则 0x11）向 EffectManager
            // 申请新对象；EffectManager 未接入前只记录种类
            distortion_kind = ins.arg_i(0);
            break;
        case 18:
            up_interp.end_time = ins.arg_i(0);
            up_interp.mode = ins.arg_i(1);
            up_interp.initial = camera.up;
            up_interp.goal = {ins.arg_f(2), ins.arg_f(3), ins.arg_f(4)};
            up_interp.timer.reset();
            break;
        case 19:
            interrupt(manager, ins.arg_i(0) + 7);
            break;
        case 20:
            distortion_radius_sq = float(ins.arg_i(0)) * float(ins.arg_i(0));
            break;
        default:
            break;
        }
        if (stopped) break;
        if (!jumped) ++ins_index;
    }
    if (!stopped) {
        // 脚本时钟按游戏速度推进（速度表槽恒为 0）
        timer.tick(game_speed);
    }
}

void StageManager::apply_rocking(float game_speed) {
    // 0x40b3b0 后半（EDI = this+0xc；0x21c pos_offset 0x228 facing_offset 0x1f8 up.x 0x200 up.z）
    const float t = rocking_timer.current_f;
    const float tp = t * kPi;
    auto finish = [&](i32 period) {
        rocking_timer.tick(game_speed);
        if (rocking_timer.current >= period) rocking_timer.set(0);
    };
    switch (rocking_mode) {
    case 1: {
        const float a = wrap_angle((tp + tp) * 0.001953125f);
        const float s = sin_f(a);
        const float px = s * -20.0f;
        camera.pos_offset.x = px;
        const float s2 = sin_f(wrap_angle(a * 2.0f)) * -10.0f;
        camera.pos_offset.z = s2;
        camera.up.x = s * -0.01f;
        camera.facing_offset.x = px * -0.5f;
        camera.facing_offset.z = s2 * -0.5f;
        finish(0x200);
        break;
    }
    case 2: {
        const float a = wrap_angle((tp + tp) / 3072.0f);
        camera.up.x = -sin_f(a);
        camera.up.z = cos_f(a);
        finish(0xc00);
        break;
    }
    case 3: {
        const float a = wrap_angle((tp + tp) * 0.00048828125f);
        const float s = sin_f(a);
        const float v = s * 50.0f;
        camera.pos_offset.x = v;
        camera.pos_offset.y = v;
        camera.pos_offset.z = sin_f(wrap_angle(a * 2.0f)) * -100.0f;
        camera.facing_offset.x = -v;
        camera.facing_offset.y = -v;
        camera.up.x = s * -0.05f;
        finish(0x800);
        break;
    }
    case 4: {
        const float a = wrap_angle((tp + tp) / 3072.0f);
        camera.up.x = -sin_f(a);
        camera.up.z = -cos_f(a);
        finish(0xc00);
        break;
    }
    case 7: {
        const float a = (tp + tp) * 0.00048828125f - kPi;
        const float s = sin_f(a);
        camera.pos_offset.x = s * 70.0f;
        camera.pos_offset.z = sin_f(wrap_angle(a + a)) * 200.0f;
        camera.up.x = s * -0.1f;
        finish(0x800);
        break;
    }
    case 8: {
        const float a = (tp + tp) * 0.0009765625f - kPi;
        const float s = sin_f(a);
        camera.pos_offset.x = s * -50.0f;
        camera.up.x = s * -0.1f;
        finish(0x400);
        break;
    }
    case 9: {
        const float a = (tp + tp) * 0.001953125f - kPi;
        const float s = sin_f(a);
        camera.up.x = s * -0.01f;
        camera.pos_offset.x = s * -15.0f;
        finish(0x200);
        break;
    }
    default:
        break;
    }
}

void StageManager::update(AnmManager& manager, float game_speed, const Rng*) {
    // 0x409e50
    if ((flags & 8) != 0) return;
    if ((flags & 4) != 0 && end_timer.current >= 0x3c) return;
    camera.shift_x = camera.shift_y = 0;
    camera.pos_delta = {0, 0, 0};
    // facing_norm = normalize(facing + facing_offset)
    {
        const float x = camera.facing.x + camera.facing_offset.x;
        const float y = camera.facing_offset.y + camera.facing.y;
        const float z = camera.facing_offset.z + camera.facing.z;
        const float len = std::sqrt(x * x + y * y + z * z);
        if (len > 0.0f) camera.facing_norm = {x / len, y / len, z / len};
        else camera.facing_norm = {x, y, z};
    }
    if ((flags & 4) == 0 || end_timer.current < 0x1e) {
        step_faces(manager);
        run_script(manager, game_speed);
        // 0x40b3b0 尾部：四组插值
        if (facing_interp.end_time != 0) camera.facing = step_interp_vec3(facing_interp, game_speed);
        if (pos_interp.end_time != 0) camera.pos = step_interp_vec3(pos_interp, game_speed);
        if (fog_interp.end_time != 0) camera.fog = step_fog(game_speed);
        if (up_interp.end_time != 0) camera.up = step_interp_vec3(up_interp, game_speed);
        if (rocking_mode != 0) apply_rocking(game_speed);
    }
    for (AnmVm& vm : layer_vms) manager.run(vm);
    ++frame;
}

void StageManager::interrupt(AnmManager& manager, i32 value) {
    // 0x40b2f0（interrupt_callback 表 DAT_00491b0c 全空）
    for (AnmVm& vm : face_vms) {
        vm.pending_interrupt = value;
        manager.run(vm);
    }
    for (AnmVm& vm : layer_vms) {
        vm.pending_interrupt = value;
        manager.run(vm);
    }
}

void StageManager::draw_layer(int layer, const DrawVisitor& visit) {
    // 0x40c280：有精灵且层号匹配的层精灵 VM
    for (int i = 0; i < kLayerSprites; ++i) {
        AnmVm& vm = layer_vms[i];
        if (vm.sprite_index < 0 || layer_ids[i] != layer) continue;
        visit(vm, true);
    }
    // 0x40af70：逐实例，对象层号匹配者按四边形顺序绘制其面 VM
    for (StdInstance& inst : std.instances) {
        StdObject& obj = std.objects[inst.object];
        if (i32(obj.layer) != layer) continue;
        obj.flags |= 2;
        inst.flags |= 1;
        ++drawn_instances;
        for (const StdQuad& q : obj.quads) {
            if (q.type != 0) continue;
            AnmVm& vm = face_vms[size_t(q.vm_index)];
            if (vm.render_mode() >= 4) {
                vm.entity_offset = {q.pos.x + inst.pos.x, q.pos.y + inst.pos.y, q.pos.z + inst.pos.z};
                if (anm != nullptr && vm.sprite_index >= 0 && size_t(vm.sprite_index) < anm->sprites.size()) {
                    const AnmRuntimeSprite& sp = anm->sprites[size_t(vm.sprite_index)];
                    if (q.width != 0.0f) { vm.flags_lo |= AnmVm::kScaleDirty; vm.scale.x = q.width / sp.width; }
                    if (q.height != 0.0f) { vm.flags_lo |= AnmVm::kScaleDirty; vm.scale.y = q.height / sp.height; }
                }
            }
            visit(vm, false);
            ++drawn_quads;
        }
    }
}

}  // namespace th16
