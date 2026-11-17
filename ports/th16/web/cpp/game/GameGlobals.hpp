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

    // 机体资源（0x42cb60 装载段 / 0x440fb0 置）
    i32 power = 0;               // DAT_004a57e4
    i32 power_max = 0;           // DAT_004a57e8 = 每级火力 × 最大等级
    i32 power_per_level = 0;     // DAT_004a57ec
    i32 lives = 2;               // DAT_004a57f4
    i32 life_fragments = 0;      // DAT_004a57f8
    i32 bombs = 3;               // DAT_004a5800
    i32 bomb_fragments = 0;      // DAT_004a5804
    i32 season_gauge = 0;        // DAT_004a5808：子季节量表（0x4440e0 以阈值表决定子机数）
    i32 season_gauge_max = 0;    // DAT_004a580c = season_threshold[7]
    i32 season_step[8] = {};     // DAT_004a5810：0x43deb0 参数
    i32 season_threshold[8] = {};// DAT_004a5838：0x43deb0 前缀和；DAT_004a583c.. = [1..6] 为子机数阈值

    // 外部推力（DAT_004d9dc4 / +4 / DAT_004d9dcc）：0x442560 每帧抄进 Player，0x441cf0 叠加到移动
    float push_x = 0, push_y = 0, push_z = 0;

    // 0x43deb0：季节阶梯 i 置 value 并重算前缀和（偶数项与奇数项分别累加后再加当前项）
    void set_season_step(int i, i32 value) {
        season_step[i] = value;
        i32 even = 0, odd = 0, last = 0, k = 0;
        if (i + 1 > 1) {
            for (; k < i; k += 2) { even += season_step[k]; odd += season_step[k + 1]; }
        }
        if (k < i + 1) last = season_step[k];
        season_threshold[i] = odd + even + last;
    }
    // 0x4440e0 / 0x40dda0：DAT_004a583c..DAT_004a5850（threshold[1..6]）中 ≤ 量表的个数
    i32 season_option_count() const {
        i32 n = 0;
        for (int i = 1; i <= 6; ++i) { if (season_gauge < season_threshold[i]) break; ++n; }
        return n;
    }

    // 0x449190：设置模式位；若原模式不是 2 则 +0x38（DAT_004a57c8）置 -1
    void set_mode(i32 mode) {
        if ((flags & kModeMask) != 0x20) replay_slot = -1;
        flags = (((u32(mode) << 4) ^ flags) & kModeMask) ^ flags;
    }
    i32 mode() const { return i32((flags & kModeMask) >> 4); }
};

}  // namespace th16
