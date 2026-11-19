// th16 成绩数据——原版 DAT_004a6f0c（0x4497e0 构造，0x1a3ac 字节；读 scoreth16.dat 失败时取默认值）。
//
// 本阶段只还原游戏内菜单（GameMenu）读写的部分：
//   0x4493c0  每机体一块 0x5318：+0x10 起 6 个难度 × 10 条排名（0x20 字节）；+0x5198 练习最高分 [难度 × 8 + 关卡]{分, 过关位}
//   0x449720  公共块（+0x19f80）：+0xc 最近输入的名字（8 空格）
//   0x43e250  排名插入：首个 score ≤ 新分 的位置，其后整体下移一格；新条目名字 8 空格、附关卡 / 续关数 / 时间 / 慢放率 / 季节
//   0x417f00 / 0x4182f0  符卡记录：机体块 +0x8d8 起每张 0x9c{名字 0x80, 取得[2], 挑战[2]}（[1] 为符卡练习）；
//             第 5 块（+0x15538 起）是全机体合计，和当前机体块同步累加；回放中不计
// 读盘（0x402440 + 0x449880 校验）/ 写盘（0x449a00）与音乐室 / 符卡记录由宿主的存档接入时补齐。
#pragma once
#include <cstring>

#include "Types.hpp"

namespace th16 {

struct ScoreEntry {
    u32 score = 0;          // +0x00
    u8 stage = 0;           // +0x04
    u8 continues = 0;       // +0x05
    char name[9] = {};      // +0x06（8 字 + NUL）
    i64 time = 0;           // +0x10（__time64）
    u32 slowdown = 0;       // +0x18：100 − 实际帧率百分比
    i32 season = 0;         // +0x1c
};

struct PracticeRecord {
    u32 score = 0;          // +0x00
    u8 cleared = 0;         // +0x04
};

struct SpellRecord {
    char name[0x80] = {};   // +0x00
    u32 captures[2] = {};   // +0x80：[0] 正常 / [1] 符卡练习
    u32 attempts[2] = {};   // +0x88
    u32 pad_90 = 0;         // +0x90
    u32 pad_94 = 0;         // +0x94
    u32 high_score = 0;     // +0x98：符卡练习最高分（0x42e150 写入；SCORE `%8d0`）
};
static_assert(sizeof(SpellRecord) == 0x9c, "SpellRecord must match scoreth16.dat 0x9c");

struct ScoreFile {
    static constexpr int kShots = 5;          // 机体块数（shot + shot_aux；块 4 为全机体合计）
    static constexpr int kAllShotsBlock = 4;
    static constexpr int kSpellRecords = 119; // +0x8d8 到练习表 +0x5198 之间能放下的符卡数
    static constexpr u32 kCounterLimit = 99999;
    static constexpr int kDifficulties = 6;
    static constexpr int kRanks = 10;
    static constexpr int kPracticeSlots = kDifficulties * 8;  // 难度 × 8 + 关卡
    static constexpr const char* kDefaultName = "        ";   // DAT_00492828

    ScoreEntry ranks[kShots][kDifficulties][kRanks];
    PracticeRecord practice[kShots][kPracticeSlots];
    u32 clears[kShots][kDifficulties] = {};          // +0x5168：各难度通关次数（上限 99999）
    u32 no_continue_clears[kShots][kDifficulties] = {};  // +0x5184：未续关通关次数
    SpellRecord spells[kShots][kSpellRecords];       // +0x8d8
    char last_name[9];                       // +0x19f8c
    // +0x19f96：8 个结局「见过」位图（bit0 见过；bit1.. 难度标记）；+0x19f9e 任一结局见过
    u8 ending_seen[8] = {};
    u8 any_ending_seen = 0;

    ScoreFile() {
        for (auto& shot : ranks)
            for (auto& difficulty : shot)
                for (int i = 0; i < kRanks; ++i) {
                    ScoreEntry& e = difficulty[i];
                    e.score = u32(100000 - 10000 * i);
                    e.stage = 1;
                    std::memcpy(e.name, "--------", 9);
                }
        std::memcpy(last_name, kDefaultName, 9);
    }

    // 0x43e250：返回插入位置，未入榜返回 -1
    i32 insert_rank(int shot, int difficulty, u32 score, u8 stage, u8 continues, i32 season, u32 slowdown, i64 time) {
        ScoreEntry* table = ranks[shot][difficulty];
        for (int rank = 0; rank < kRanks; ++rank) {
            if (table[rank].score > score) continue;
            for (int i = kRanks - 1; i > rank; --i) table[i] = table[i - 1];
            ScoreEntry& e = table[rank];
            e.score = score;
            e.stage = stage;
            e.continues = continues;
            e.time = time;
            std::memcpy(e.name, kDefaultName, 9);
            e.slowdown = slowdown;
            e.season = season;
            return rank;
        }
        return -1;
    }

    // 0x417f00 符卡开始（非回放）：当前机体块与合计块都登记名字、挑战数 +1
    void record_spell_attempt(int shot, int spell, bool practice, const char* name) {
        if (!spell_record_valid(shot, spell)) return;
        for (int block : {shot, kAllShotsBlock}) {
            SpellRecord& record = spells[block][spell];
            std::strncpy(record.name, name, sizeof(record.name) - 1);
            record.name[sizeof(record.name) - 1] = '\0';
            if (record.attempts[practice] < kCounterLimit) ++record.attempts[practice];
        }
    }
    // 0x4182f0 符卡取得（非回放）：两块取得数 +1
    void record_spell_capture(int shot, int spell, bool practice) {
        if (!spell_record_valid(shot, spell)) return;
        for (int block : {shot, kAllShotsBlock}) {
            SpellRecord& record = spells[block][spell];
            if (record.captures[practice] < kCounterLimit) ++record.captures[practice];
        }
    }
    const SpellRecord* spell_record(int shot, int spell) const {
        return spell_record_valid(shot, spell) ? &spells[shot][spell] : nullptr;
    }
    static bool spell_record_valid(int shot, int spell) {
        return shot >= 0 && shot < kAllShotsBlock && spell >= 0 && spell < kSpellRecords;
    }

    // 0x42e150 第 6 面 / Extra 通关（非回放）：通关次数 +1，未续关再 +1
    void record_clear(int shot, int difficulty, bool no_continue) {
        if (clears[shot][difficulty] < 99999) ++clears[shot][difficulty];
        if (no_continue && no_continue_clears[shot][difficulty] < 99999) ++no_continue_clears[shot][difficulty];
    }
    // 0x42e150 过关（非回放、非符卡练习）：练习选关解锁位 +0x519c
    void mark_practice_cleared(int shot, int difficulty, int stage) { practice[shot][difficulty * 8 + stage].cleared = 1; }

    // 0x43f7e0 练习分支：练习最高分只升不降
    void update_practice(int shot, int difficulty, int stage, u32 score) {
        PracticeRecord& r = practice[shot][difficulty * 8 + stage];
        if (r.score < score) r.score = score;
    }

    // 0x42e150 符卡练习：+0x970 最高分只升不降（分值先 /10*10）
    void update_spell_high_score(int shot, int spell, u32 score) {
        if (!spell_record_valid(shot, spell)) return;
        const u32 floored = (score / 10u) * 10u;
        SpellRecord& record = spells[shot][spell];
        if (record.high_score < floored) record.high_score = floored;
    }
};

}  // namespace th16
