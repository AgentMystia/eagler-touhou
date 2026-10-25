// BossPresentation 实现：0x4265a0 boss 段的逐条移植（行号对照见头注释）。
// VM 写入走 AnmRegistry 句柄（不旁路池调度）；无脚本的星位/位标 VM 与
// 原版空白模板 VM 同构（0x403800 复制的池模板无脚本——状态位持有者）。
#include "BossPresentation.hpp"

#include <cmath>

namespace th13 {

namespace {
// 原版 VM 字段位（00462450.c/0046a700.c 消费位）：
constexpr u32 kFlagVisible = 0x2;    // VM+0x18 bit1（0x4265a0:102-112 显隐位）
constexpr u32 kFlagSelected = 0x1;   // VM+0x18 bit0（ins_300 精灵选中位）
constexpr float kPi = 3.1415927410125732421875f;
constexpr float kTwoPi = 6.283185482025146484375f;
}  // namespace

bool BossPresentation::initialize() {
    if (digit_tens_ && digit_ones_) return true;
    // 0x425170:181-287：ascii 池模板 2/3 → ctx+0x98/+0x9c，初始隐藏
    //（VM+0x18 &= ~2、VM+0x1c &= ~0x40000 对应 render_flags 位清）。
    digit_tens_ = animations_.create(banks_.ascii, kTimerTensScript, -1, 0);
    digit_ones_ = animations_.create(banks_.ascii, kTimerOnesScript, -1, 0);
    if (!digit_tens_ || !digit_ones_) {
        fail("Boss timer digit animation unavailable");
        return false;
    }
    for (u32 handle : {digit_tens_, digit_ones_}) {
        if (auto* vm = animations_.registry.find(handle)) {
            vm->visual.flags &= ~kFlagVisible;
            vm->visual.render_flags &= ~0x40000u;
        }
    }
    return true;
}

void BossPresentation::release() {
    retire_ring();
    for (auto& handle : star_handles_) {
        if (handle && !animations_.retire(handle)) fail("Boss star retire failed");
        handle = 0;
    }
    stars_created_ = false;
    for (u32 handle : {digit_tens_, digit_ones_, marker_handle_}) {
        if (handle && !animations_.retire(handle)) fail("Boss VM retire failed");
    }
    digit_tens_ = digit_ones_ = marker_handle_ = 0;
    gate_ = false;
    displayed_ratio_ = target_ratio_ = 0;
    timer_displayed_ = -1;
    warn_state_ = 0;
    escalate_state_ = 0;
}

bool BossPresentation::create_ring() {
    // 槽 240/241/242 → front.anm 240/241/242（0x426b06/0x426bad/0x426c66）。
    if (ring_created_) return true;
    ring_arc_ = animations_.create(banks_.front, kRingArcScript, -1, 0);
    if (!ring_arc_) {
        fail("Boss ring arc animation unavailable");
        return false;
    }
    for (i32 i = 0; i < 2; i++) {
        ring_overlays_[u32(i)] = animations_.create(banks_.front, kRingOverlayScripts[i], -1, 0);
        if (!ring_overlays_[u32(i)]) {
            fail("Boss ring overlay animation unavailable");
            return false;
        }
    }
    for (u32 handle : {ring_arc_, ring_overlays_[0], ring_overlays_[1]})
        if (auto* vm = animations_.registry.find(handle))
            vm->visual.render_flags &= ~0xc000u;
    ring_created_ = true;
    return true;
}

void BossPresentation::retire_ring() {
    for (u32 handle : {ring_arc_, ring_overlays_[0], ring_overlays_[1]}) {
        if (handle && !animations_.retire(handle)) fail("Boss ring retire failed");
    }
    ring_arc_ = ring_overlays_[0] = ring_overlays_[1] = 0;
    ring_created_ = false;
}

bool BossPresentation::create_stars() {
    // 槽 243×4 → front.anm 243（0x426d13/0x426dba/0x426e61/0x426f1a）。
    if (stars_created_) return true;
    for (auto& handle : star_handles_) {
        handle = animations_.create(banks_.front, kRingStarScript, -1, 0);
        if (!handle) {
            fail("Boss star animation unavailable");
            return false;
        }
        if (auto* vm = animations_.registry.find(handle))
            vm->visual.render_flags &= ~0xc000u;
    }
    stars_created_ = true;
    return true;
}

bool BossPresentation::ensure_marker() {
    if (marker_handle_) return true;
    // 0x425170:447-508：ctx+0xa0 常备空白 VM（位标字节 +0x52b 驱动者）。
    auto* vm = animations_.registry.allocate();
    if (!vm) {
        fail("Boss marker pool exhausted");
        return false;
    }
    vm->initialize_unbound();
    marker_handle_ = animations_.registry.submit(*vm);
    if (!marker_handle_) {
        fail(animations_.registry.error.c_str());
        return false;
    }
    return true;
}

void BossPresentation::spell_started() {
    // 0x412890：两数字 VM pending_interrupt = 2。
    for (u32 handle : {digit_tens_, digit_ones_}) {
        if (auto* vm = animations_.registry.find(handle)) vm->pending_interrupt = 2;
    }
}

void BossPresentation::interrupt_digits(i32 label) {
    // 0x42b510/0x42b530 同式（label 4=复原、5=移开）。
    for (u32 handle : {digit_tens_, digit_ones_}) {
        if (auto* vm = animations_.registry.find(handle)) vm->pending_interrupt = label;
    }
}

u32 BossPresentation::timer_digit_color() const {
    auto* vm = const_cast<AnmManager&>(animations_).registry.find(digit_tens_);
    return vm ? vm->visual.color : 0xffffffffu;
}

bool BossPresentation::marker_visible() const noexcept {
    // 门 + 敌位标显隐（0x4265a0:630-739：boss 位 0x521c bit0/bit5 均 0）。
    return gate_;
}

bool BossPresentation::update(const BossFrameInput& input,
                              const std::function<bool(i32)>& sound) {
    const StageEnemy* boss = input.boss;
    // 0x4265a0:98-107 门控：boss 槽存活 && !boss_leaving && !dialog && 计时>=0。
    const bool gate = boss != nullptr && !input.boss_leaving && !input.dialog_active &&
                      input.timer_seconds >= 0;
    if (gate != gate_) {
        gate_ = gate;
        // 0x4265a0:102-112：数字 VM 显隐位（VM+0x18 bit1）。
        for (u32 handle : {digit_tens_, digit_ones_}) {
            if (auto* vm = animations_.registry.find(handle)) {
                if (gate)
                    vm->visual.flags |= kFlagVisible | kFlagSelected;
                else
                    vm->visual.flags &= ~kFlagVisible;
            }
        }
        if (!gate) {
            // 0x4265a0:100-107 失活支路：环组/星 VM 回收（0x46fc30×7）。
            retire_ring();
            for (auto& handle : star_handles_) {
                if (handle && !animations_.retire(handle))
                    fail("Boss star retire failed on gate close");
                handle = 0;
            }
            stars_created_ = false;
            if (auto* vm = animations_.registry.find(marker_handle_))
                vm->visual.flags &= ~kFlagVisible;
            displayed_ratio_ = target_ratio_ = 0;
        }
    }
    if (!gate || !boss) return true;

    // ---- 计时秒数与警告（0x4265a0:147-177）----
    timer_seconds_ = input.timer_seconds;
    timer_frames_ = input.timer_frames;
    const i32 seconds = input.timer_seconds;
    if (seconds != timer_displayed_) {
        if (seconds < timer_displayed_) {
            // 递减：<5 声 0xc、<10 声 0xb（0x460bc0(vm,id)）+ 数字 VM 中断
            //（0x4126b0 的 EDI 标签未还原，仅保留声与精灵刷新）。
            if (seconds < 5) {
                if (warn_state_ != 2) {
                    warn_state_ = 2;
                    if (sound && !sound(0xc)) fail("Boss timer sound service failed");
                    interrupt_digits(4);
                }
            } else if (seconds < 10) {
                if (warn_state_ < 1) {
                    warn_state_ = 1;
                    if (sound && !sound(0xb)) fail("Boss timer sound service failed");
                    interrupt_digits(4);
                }
            }
        } else {
            // 复位/增长：仅中断（0x4265a0:160-163）。
            interrupt_digits(4);
            warn_state_ = 0;
        }
        // 数字精灵 = 270 + 数位（0x461a60 同式：sprite 直选 + uv/矩阵重算）。
        if (auto* vm = animations_.registry.find(digit_tens_))
            if (!vm->select_sprite(kTimerDigitSpriteBase + seconds / 10))
                fail("Boss timer tens sprite unavailable");
        if (auto* vm = animations_.registry.find(digit_ones_))
            if (!vm->select_sprite(kTimerDigitSpriteBase + seconds % 10))
                fail("Boss timer ones sprite unavailable");
        timer_displayed_ = seconds;
    }

    // ---- 玩家贴顶移动态机（0x4265a0:109-145，迟滞 128/160）----
    const u32 move = (move_state_ >> 9) & 3;
    if (move == 0) {
        if (input.player_y < 128.f) {
            move_state_ = (move_state_ & ~0x400u) | 0x200u;
            interrupt_digits(5);  // 0x42b530：移开
        }
    } else if (move == 1) {
        if (input.player_y > 160.f) {
            interrupt_digits(4);  // 0x42b510：复原
            move_state_ &= ~0x600u;
        }
    }

    // ---- 血环比值（0x4265a0:225-239）----
    if (input.life_initial > 0 && input.boss_life >= 0) {
        target_ratio_ = float(input.boss_life) / float(input.life_initial);
        if (target_ratio_ > 1.f) target_ratio_ = 1.f;
    }
    // displayed：目标更小直落、否则每帧 +0.025 逼近（0x4265a0:230-239）。
    if (displayed_ratio_ > target_ratio_)
        displayed_ratio_ = target_ratio_;
    else
        displayed_ratio_ = float(displayed_ratio_ + 0.025f) > target_ratio_
                               ? target_ratio_
                               : displayed_ratio_ + 0.025f;

    // ---- 玩家邻近隐藏（0x4265a0:484-516，迟滞 80/96）----
    {
        const float dx = boss->position[0] - input.player_x;
        const float dy = boss->position[1] - input.player_y;
        const float distance_squared = dx * dx + dy * dy;
        if (!proximity_hidden_ && distance_squared < 6400.f) {
            proximity_hidden_ = true;
            retire_ring();
            for (auto& handle : star_handles_) {
                if (handle && !animations_.retire(handle)) fail("Boss star retire failed");
                handle = 0;
            }
            stars_created_ = false;
        } else if (proximity_hidden_ && distance_squared >= 9216.f) {
            proximity_hidden_ = false;  // 本帧环段按常态重建
        }
    }

    // ---- 环根与跟随（0x4265a0:416-447 + 0x426f9e-0x427091 写入点）----
    const bool life_ratio_ready = input.life_initial > 0 && !proximity_hidden_;
    if (life_ratio_ready) {
        if (!create_ring()) return false;
        // 驱动：VM+0x18|=4、rotation.x = -displayed*2π（0x426fcc-0x426fd8）。
        if (auto* vm = animations_.registry.find(ring_arc_)) {
            vm->visual.flags |= 0x4u;
            vm->variables.rotation.x = -displayed_ratio_ * kTwoPi;
            // 三条带 offset(+0x574..) = boss 世界位（0x426fdb-0x42704f）。
            vm->visual.translation = {boss->position[0], boss->position[1],
                                      boss->position[2]};
        }
        for (u32 handle : ring_overlays_)
            if (auto* vm = animations_.registry.find(handle))
                vm->visual.translation = {boss->position[0], boss->position[1],
                                          boss->position[2]};
    } else if (ring_created_) {
        retire_ring();
    }

    // ---- 阶段星（0x4265a0:448-482）----
    const bool markers_present = input.markers[0].ratio > 0 || input.markers[1].ratio > 0 ||
                                 input.markers[2].ratio > 0 || input.markers[3].ratio > 0;
    if (markers_present && !proximity_hidden_) {
        if (!stars_created_ && !create_stars()) return false;
        for (i32 i = 0; i < 4; i++) write_star(star_handles_[u32(i)], input, i);
    } else if (stars_created_ && (!markers_present || proximity_hidden_)) {
        for (auto& handle : star_handles_) {
            if (handle && !animations_.retire(handle)) fail("Boss star retire failed");
            handle = 0;
        }
        stars_created_ = false;
    }

    // ---- 底部敌位标（0x4265a0:630-739）----
    if (!ensure_marker()) return false;
    if (auto* vm = animations_.registry.find(marker_handle_)) {
        vm->visual.flags |= kFlagVisible | kFlagSelected;
        // offset = (boss_x + 32 + 192, 480)（0x4265a0:717-719）。
        vm->visual.translation = {boss->position[0] + 32.f + 192.f, 480.f, 0.f};
        // 字节 +0x52b = '@' + round(|dx|*191/64)；|dx|>=64 → 0xff、横向出界 → 0。
        // 原版该字节由 ascii 字模层消费；本作 AnmVm 无对应槽，落本类状态面
        //（marker_glyph_，文本层接线后消费）。
        const float dx = std::fabs(boss->position[0] - input.player_x);
        i32 glyph = 0xff;
        if (dx < 64.f) glyph = '@' + i32(std::nearbyint(dx * 191.f * 0.015625f));
        if (boss->position[0] < -192.f || boss->position[0] > 192.f) glyph = 0;
        marker_glyph_ = glyph;
        // 计时衰减阶段中断（0x4265a0:640-716：<700/400/200（trance 2000/1000/400）；
        // 帧值口径待 StageSim 供给，暂以秒 60fps 换算驱动态机）。
        const i32 frames_left = seconds * 60;
        const i32 t1 = input.trance_mode ? 2000 : 700;
        const i32 t2 = input.trance_mode ? 1000 : 400;
        const i32 t3 = input.trance_mode ? 400 : 200;
        if (escalate_state_ == 0 && frames_left < t1) {
            vm->pending_interrupt = 7;
            escalate_state_ = 1;
        } else if (escalate_state_ == 1 && frames_left < t2) {
            vm->pending_interrupt = 8;
            escalate_state_ = 2;
        } else if (escalate_state_ == 2 && frames_left < t3) {
            if (sound && !sound(0x2e)) fail("Boss escalate sound service failed");
            escalate_state_ = 3;
        }
    }
    return true;
}

void BossPresentation::write_star(u32 handle, const BossFrameInput& input, i32 index) {
    auto* vm = animations_.registry.find(handle);
    if (!vm) return;
    const float angle = input.markers[u32(index)].ratio;
    // 0x4270be-0x4270fa：angle==0 或 displayed >= angle → 隐藏（清 bit1）。
    if (angle == 0.f || displayed_ratio_ >= angle) {
        vm->visual.flags &= ~kFlagVisible;
        return;
    }
    vm->visual.flags |= kFlagVisible | kFlagSelected;
    // 0x427100-0x427139：bit1|bit2 置、rotation.z = normalize(−π − a*2π)
    //（0x4ae880=−π、0x4ae850=2π、0x472f30 归一）。
    const float t = normalize_angle(float(-kPi - angle * kTwoPi));
    vm->variables.rotation.z = t;
    // 0x42713c-0x427193：offset = boss + 56·(cos t, sin t, 0)
    //（0x4aef18=56、0x472fc0 极坐标旋转基向量 (56,0)）。
    const StageEnemy* boss = input.boss;
    vm->visual.translation = {
        float(boss->position[0] + kStarOrbitRadius * std::cos(t)),
        float(boss->position[1] + kStarOrbitRadius * std::sin(t)),
        boss->position[2]};
}

}  // namespace th13
