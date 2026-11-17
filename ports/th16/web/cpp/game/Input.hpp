// th16 输入采样——原版 0x401d50（键盘/手柄位收集）+ 0x405580（边沿与连发）。
// 位布局与 DAT_004a50b0 一致：bit0 射击/确定、bit1 炸弹、bit3 取消、bit8 暂停…；
// 启动幕跳过检测用的 DAT_004c1800 & 0x180 与「确定|取消」同口径（见 0x44ac00）。
#pragma once
#include "Types.hpp"

namespace th16 {

struct InputState {
    static constexpr u32 kShot = 1u << 0;
    static constexpr u32 kBomb = 1u << 1;
    static constexpr u32 kSlow = 1u << 2;
    static constexpr u32 kCancel = 1u << 3;
    static constexpr u32 kUp = 1u << 4;
    static constexpr u32 kDown = 1u << 5;
    static constexpr u32 kLeft = 1u << 6;
    static constexpr u32 kRight = 1u << 7;
    static constexpr u32 kSkip = 1u << 8;   // 与取消一起构成启动幕 0x180 跳过掩码的高半
    static constexpr u32 kBootSkipMask = 0x180u;  // DAT_004c1800 测试掩码（0x44ac00）

    u32 held = 0;       // DAT_004a50b0 当前帧
    u32 previous = 0;   // DAT_004a50b4 上一帧
    u32 pressed = 0;    // 上升沿（0x405580 +0xc）
    u32 released = 0;   // 下降沿（0x405580 +0x10）

    // 0x405580：根据 held/previous 刷新边沿
    void commit() {
        pressed = (previous ^ held) & held;
        released = (previous ^ held) & previous;
        previous = held;
    }

    bool boot_skip() const { return (held & (kShot | kCancel | kSkip)) != 0 || (pressed & (kShot | kCancel | kSkip)) != 0; }
};

}  // namespace th16
