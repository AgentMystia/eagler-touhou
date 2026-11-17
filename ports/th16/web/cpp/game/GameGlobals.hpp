// th16 全局游戏配置——原版散落的 .data 全局（初值按 th16.exe .data 节读取）。
#pragma once
#include "Types.hpp"

namespace th16 {

struct GameGlobals {
    static constexpr u32 kModeMask = 0x30;  // DAT_004a5bec bit4-5：0 普通 / 1 练习 / 2 符卡练习（0x449190）

    i32 stage = 0;               // DAT_004a5790
    i32 stage_copy = 0;          // DAT_004a5794
    i32 shot = 0;                // DAT_004a57a4 机体（0 灵梦 1 琪露诺 2 文 3 魔理沙）
    i32 shot_aux = 0;            // DAT_004a57a8
    i32 season = 0;              // DAT_004a57ac 子季节（0 春 1 夏 2 秋 3 冬 4 土用=Extra）
    i32 difficulty = 1;          // DAT_004a57b4（初值 1 = Normal；4 = Extra）
    i32 replay_slot = 0;         // DAT_004a57c8
    u32 flags = 0;               // DAT_004a5bec（bit4-5 模式；bit6 demo 返回；bit7）
    i32 last_difficulty = 1;     // DAT_0049f274
    i32 last_shot = 0;           // DAT_004a6f24
    i32 last_practice_stage = -1;// DAT_004a2974
    i32 title_origin = 0;        // DAT_004a6f1c：进入标题的来源（0 首次 1 正常 2 Replay 结束 4 练习返回 5 符卡练习返回）
    i32 stage_table = 0;         // DAT_004a6f18：0 = &DAT_004a23a4 普通关卡表，1 = &DAT_004a289c Extra 表（按季节表时另记）
    i32 scene_request = -1;      // DAT_004c17c4：Supervisor 场景请求（7 = Game）
    i32 game_frame = 0;          // DAT_004a579c：本局帧计数（0x42d7b0 每帧 +1，0x42dc50 清零）
    i32 stage_frame = 0;         // DAT_004a57a0：本关帧计数

    // 0x449190：设置模式位；若原模式不是 2 则 +0x38（DAT_004a57c8）置 -1
    void set_mode(i32 mode) {
        if ((flags & kModeMask) != 0x20) replay_slot = -1;
        flags = (((u32(mode) << 4) ^ flags) & kModeMask) ^ flags;
    }
    i32 mode() const { return i32((flags & kModeMask) >> 4); }
};

}  // namespace th16
