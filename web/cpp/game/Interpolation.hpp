// th13 ECL 插值通道与缓动权重。
// 通道对象固定 0x30 字节：原版线程状态块（§2.1，docs/plans/ecl-vm.md）在 +0x1020 起
// 内联 8×0x30 通道（0x417450 初始化 8 组；解释器 0x4772a0 case 0x5b/0x5c 挂接），
// 通道内部字节布局未逐字段定域，此处为行为等价的紧凑布局，供 EclFile 快照落盘。
// 缓动模式族沿用 th13 世代 ZUN 共享表（th15 Interpolation.cpp 已对齐的同族实现，
// 待 oracle 对照轮逐模式验证）。
#pragma once

#include "Types.hpp"

#include <array>

namespace th13 {

// 帧计时器：整数帧 + 浮点分数双轨，按每帧步长 rate 推进。
struct Timer {
    i32 previous = -999999;
    i32 current = 0;
    float fractional = 0;
    u32 flags = 0;

    void set(i32 frame) noexcept {
        if (!(flags & 1)) {
            current = 0;
            previous = -999999;
            fractional = 0;
            flags |= 1;
        }
        current = frame;
        previous = wrapping_add(frame, -1);
        fractional = static_cast<float>(frame);
    }

    void tick(float rate) noexcept {
        previous = current;
        if (rate > 0.99f && rate < 1.01f) {
            current = wrapping_add(current, 1);
            fractional = static_cast<float>(fractional + 1.f);
        } else {
            fractional = static_cast<float>(fractional + rate);
            current = truncate_int(fractional);
        }
    }
};
static_assert(sizeof(Timer) == 16);

// 标量插值通道（ECL 指令 91/92 挂接；0x30 字节见文件头说明）。
struct EclInterpolation {
    i32 duration = 0;
    i32 mode = 0;
    float start = 0, end = 0, control1 = 0, control2 = 0, current = 0;
    u32 reserved = 0;  // 通道块凑满 0x30（§2.1 每通道 0x30 字节）
    Timer timer{};

    void begin(i32 frames, i32 easing, float from, float to) noexcept {
        start = from;
        end = to;
        duration = frames;
        mode = easing;
        timer.set(0);
    }

    float step(float rate) noexcept;
    float evaluate() noexcept;
};
static_assert(sizeof(EclInterpolation) == 0x30);

// 缓动权重：mode 决定 [0,1] 进度的映射。
float interpolation_weight(i32 mode, float elapsed, float duration) noexcept;

}  // namespace th13
