// th16 .sht 射击表——原版 0x443790 装载（pl0N.sht 机体表 / pl0Nsub.sht 子季节表）。
//
// 文件布局（0x443790 + 0x440fb0 / 0x4440e0 / 0x4456d0 的读法）：
//   +0x00 i16            未用（恒 4）
//   +0x02 i16            射击组数（count）
//   +0x04 f32            判定半径（装载后被 exe 表 DAT_00492c98[shot] 覆盖 = 3.0）
//   +0x08 f32            擦弹半径（被 DAT_00492c78[shot] 覆盖 = 5.0）
//   +0x0c f32            收点半径（被 DAT_00492c88[shot] 覆盖 = 60.0）
//   +0x10..+0x1c f32×4   速度：非低速 / 低速 / 斜向非低速 / 斜向低速（0x440fb0 ×128 取整）
//   +0x20 i32            最大火力等级（4）
//   +0x24 i32            每级火力（装载后被 0x440fb0 覆盖为 100）
//   +0x40 f32×2 ×21      子机位置（非低速），+0xe8 ×21 低速；下标来自 DAT_004a5e4c / DAT_004a5dac 表
//   +0x190 i32 ×count    各射击组相对 +0x1b8 的偏移
//   每组：0x58 字节条目若干，首字节 < 0 结束；+0x28/+0x2c/+0x30/+0x34 为回调表下标
//   （DAT_004919c0 / DAT_004919a0 / DAT_004a6f04 / DAT_00491980），装载时就地换成函数指针
#pragma once
#include "Types.hpp"

#include <string>
#include <vector>

namespace th16 {

struct ShotEntry {
    static constexpr u32 kSize = 0x58;
    u8 raw[kSize] = {};  // 射击系统（0x4456d0 / 0x445470）接入时再细分字段

    i32 i32_at(u32 off) const { return read_i32(raw + off); }
    float f32_at(u32 off) const { return read_f32(raw + off); }
};

struct ShotTable {
    static constexpr int kOptionSlots = 21;

    i16 header = 0;
    i16 set_count = 0;
    float hitbox = 0;        // +0x04
    float graze = 0;         // +0x08
    float item = 0;          // +0x0c
    float speed = 0;         // +0x10
    float speed_focus = 0;   // +0x14
    float speed_diag = 0;    // +0x18
    float speed_diag_focus = 0;  // +0x1c
    i32 max_level = 0;       // +0x20
    i32 power_per_level = 0; // +0x24
    Vec2 option_unfocused[kOptionSlots];  // +0x40
    Vec2 option_focused[kOptionSlots];    // +0xe8
    std::vector<std::vector<ShotEntry>> sets;
    std::string error;

    bool parse(const u8* bytes, u32 size);
};

}  // namespace th16
