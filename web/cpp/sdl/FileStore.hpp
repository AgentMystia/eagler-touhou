// 文件存储（真实持久化，替换旧桩：不再 return true 假成功）。
// 数据 = 原版 scoreth13.dat 的移植侧子集（v2；兼容 v1）：
//   - BGM/SE 音量（原版存档字节 DAT_004dc892/893，默认 100/80 = 0x5064）。
//   - 每角色×难度通关标记 cleared(char 0..3, diff 0..4)：原版 score 数据块
//     布局 *(int*)(score + 0x56dc*char + 0x7e4 + diff*4) != 0（0x451450
//     case0 角色 クリア済 札门实证）；新档全 0 = 全隐，与真机缺省一致。
// 后端：native/WASI = 真实文件（TH13_SAVE_PATH 或 cwd th13-save.bin）；
// web = localStorage("th13.save")（EM_JS）。失败如实置 error 返回 false。
// 损坏/缺档：按原版 scoreth13.dat 语义取默认值，不报错。
#pragma once

#include "../game/Types.hpp"
#include <string>
#include <array>

namespace th13::sdl {

inline constexpr i32 kSaveCharacters = 4;   // 霊夢/魔理沙/早苗/妖夢
inline constexpr i32 kSaveDifficulties = 5; // Easy..Lunatic(0..3) + Extra(4)

class FileStore {
public:
    std::string error;
    i32 bgm_volume = 100, effect_volume = 80;
    u32 cleared_mask = 0;  // bit = char*kSaveDifficulties + diff
    // v1 reserved[0]（buffer+24）：MusicRoom 曲目听过位（bit = 曲号，0x4554f0
    // 落档族；原版 score 块 +0x1b27a 字节 0 语义：新档全 0 = 全 "???"，
    // track0 由首次播曲解锁）。格式不变（v1 36B 布局原即保留 3 u32）。
    u32 heard_mask = 0;
    // v2 extension: 4 characters * 4 ordinary ranks, bits0..5 = stages1..6.
    // v1 has no played data, so every bit remains zero when loading old saves.
    std::array<u8, 16> practice_played{};
    bool practice_stage_played(i32 character, i32 difficulty, i32 stage) const;
    void mark_practice_stage_played(i32 character, i32 difficulty, i32 stage);
    // Extra 解锁位（原版 score 块 0x1b26b+char*2 bit0x10：0x416130 通关落档
    // diff>0 时置位、0x44bec0 读侧；0x44c570 case0 经 +0x5c40 bit1 门 Extra
    // 行 + 0x44cd10/0x451450 的 rank 屏 5 行分支）。落 v2 reserved[0]
    // （buffer+36）低 4 位，不进 v1 兼容面。
    u32 extra_unlocked_mask = 0;
    bool extra_unlocked(i32 character) const {
        return character >= 0 && character < 4 && (extra_unlocked_mask >> character) & 1u;
    }
    void set_extra_unlocked(i32 character) {
        if (character >= 0 && character < 4) extra_unlocked_mask |= 1u << character;
    }
    bool loaded = false;   // true = 读到并采用合法档；false = 新档默认值

    bool initialize();  // 装载（缺档/坏档 = 默认值；I/O 硬失败 false + error）
    bool save();        // 真实写入；失败 false + error（绝不假成功）

    bool cleared(i32 character, i32 difficulty) const;
    void set_cleared(i32 character, i32 difficulty);  // 通关落档（bit 置位）
    // MusicRoom 听过位（musicroom-host.md §2 建议的内联读写面）。
    bool music_heard(i32 track) const {
        return track >= 0 && track < 32 && (heard_mask >> u32(track)) & 1u;
    }
    void set_music_heard(i32 track) {
        if (track >= 0 && track < 32) heard_mask |= 1u << u32(track);
    }
};

}  // namespace th13::sdl
