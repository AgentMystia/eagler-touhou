// th16 关卡表——原版 DAT_004a22d0 起 8 条 × 0xd4 字节（0x43ce10 以 DAT_004a5794 选条目写 DAT_004a6f18）。
//
//   +0x00 i32 index  +0x04 std 文件名  +0x08 ecl 文件名  +0x0c / +0x10 BGM 名（thbgm.fmt 曲目；0x42cb60 以
//   0x43c370(0, bgm0) / (1, bgm1) 装载）  +0x14..+0x20 msg 文件名 ×4（按机体）  +0x24 关卡 logo ANM
//   +0x28 i32 BGM 曲目号（0x43c3f0(0, 曲目) 播放；0x42dee0 以此解锁音乐室）  +0x2c i32 Boss 曲目号
//   其余为关卡专属整数参数。
// 条目 0（st00.ecl + st01.std）为内部测试关；正常游戏从条目 1 起。
#pragma once
#include "Types.hpp"

namespace th16 {

struct StageEntry {
    i32 index;
    const char* std_name;
    const char* ecl_name;
    const char* bgm_name;
    const char* bgm_boss_name;
    const char* msg_names[4];
    const char* logo_anm;
    i32 bgm_track;
    i32 bgm_boss_track;
};

inline constexpr StageEntry kStageTable[8] = {
    {0, "st01.std", "st00.ecl", "th16_02", "th16_03", {"st00a.msg", "st01b.msg", "st01c.msg", "st01d.msg"}, "st01logo.anm", 1, 2},
    {1, "st01.std", "st01.ecl", "th16_02", "th16_03", {"st01a.msg", "st01b.msg", "st01c.msg", "st01d.msg"}, "st01logo.anm", 1, 2},
    {2, "st02.std", "st02.ecl", "th16_04", "th16_05", {"st02a.msg", "st02b.msg", "st02c.msg", "st02d.msg"}, "st02logo.anm", 3, 4},
    {3, "st03.std", "st03.ecl", "th16_06", "th16_07", {"st03a.msg", "st03b.msg", "st03c.msg", "st03d.msg"}, "st03logo.anm", 5, 6},
    {4, "st04.std", "st04.ecl", "th16_08", "th16_09", {"st04a.msg", "st04b.msg", "st04c.msg", "st04d.msg"}, "st04logo.anm", 7, 8},
    {5, "st05.std", "st05.ecl", "th16_10", "th16_11", {"st05a.msg", "st05b.msg", "st05c.msg", "st05d.msg"}, "st05logo.anm", 9, 10},
    {6, "st06.std", "st06.ecl", "th16_13", "th16_12", {"st06a.msg", "st06b.msg", "st06c.msg", "st06d.msg"}, "st06logo.anm", 11, 12},
    {7, "st07.std", "st07.ecl", "th16_16", "th16_17", {"st07a.msg", "st07b.msg", "st07c.msg", "st07d.msg"}, "st07logo.anm", 13, 14},
};

// DAT_004a6f18 = &DAT_004a22d0 + DAT_004a5794 * 0xd4
inline const StageEntry& stage_entry(i32 stage) {
    return kStageTable[(stage < 0 || stage > 7) ? 0 : stage];
}

}  // namespace th16
