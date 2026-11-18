// th16 成绩数据——原版 DAT_004a6f0c（0x4497e0 构造，0x1a3ac 字节；读 scoreth16.dat 失败时取默认值）。
//
// 本阶段只还原游戏内菜单（GameMenu）读写的部分：
//   0x4493c0  每机体一块 0x5318：+0x10 起 6 个难度 × 10 条排名（0x20 字节）；+0x5198 练习最高分 [难度 × 8 + 关卡]{分, 过关位}
//   0x449720  公共块（+0x19f80）：+0xc 最近输入的名字（8 空格）
//   0x43e250  排名插入：首个 score ≤ 新分 的位置，其后整体下移一格；新条目名字 8 空格、附关卡 / 续关数 / 时间 / 慢放率 / 季节
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

struct ScoreFile {
    static constexpr int kShots = 5;          // 机体块数（shot + shot_aux）
    static constexpr int kDifficulties = 6;
    static constexpr int kRanks = 10;
    static constexpr int kPracticeSlots = kDifficulties * 8;  // 难度 × 8 + 关卡
    static constexpr const char* kDefaultName = "        ";   // DAT_00492828

    ScoreEntry ranks[kShots][kDifficulties][kRanks];
    PracticeRecord practice[kShots][kPracticeSlots];
    u32 clears[kShots][kDifficulties] = {};          // +0x5168：各难度通关次数（上限 99999）
    u32 no_continue_clears[kShots][kDifficulties] = {};  // +0x5184：未续关通关次数
    char last_name[9];                       // +0x19f8c

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
};

}  // namespace th16
