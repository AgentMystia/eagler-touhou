// th16 符卡练习条目表——原版 DAT_00490ee0（0x456060 解锁判定 / 0x4560b0 装列表）。
// 每组最多 0xd 槽，每槽最多 5 个符卡 id（-1 终止）；组大小见 kGroupCounts。
#pragma once
#include "ScoreFile.hpp"
#include "Types.hpp"

namespace th16 {

struct SpellPracticeTable {
    static constexpr int kGroups = 7;
    static constexpr int kSlots = 0xd;
    static constexpr int kIdsPerSlot = 5;
    // 0x455900 local_28
    static constexpr i32 kGroupCounts[kGroups] = {2, 3, 4, 3, 6, 9, 0xd};

    // DAT_00490ee0：((group * 0xd + slot) * 5) 个 i32
    static constexpr i32 kIds[kGroups][kSlots][kIdsPerSlot] = {
        {{0, 1, 2, 3, -1}, {4, 5, 6, 7, -1}},
        {{8, 9, 10, 11, -1}, {12, 13, 14, 15, -1}, {16, 17, 18, 19, -1}},
        {{20, 21, -1, -1, -1}, {22, 23, 24, 25, -1}, {26, 27, 28, 29, -1}, {30, 31, 32, 33, -1}},
        {{34, 35, 36, 37, -1}, {38, 39, 40, 41, -1}, {42, 43, 44, 45, -1}},
        {{46, 47, 48, 49, -1}, {50, 51, 52, 53, -1}, {54, 55, 56, 57, -1},
         {58, 59, 60, 61, -1}, {62, 63, 64, 65, -1}, {66, 67, 68, 69, -1}},
        {{70, 71, 72, 73, -1}, {74, 75, 76, 77, -1}, {78, 79, 80, 81, -1}, {82, 83, 84, 85, -1},
         {86, 87, 88, 89, -1}, {90, 91, 92, 93, -1}, {94, 95, 96, 97, -1}, {98, 99, 100, 101, -1},
         {102, 103, 104, 105, -1}},
        {{106, -1, -1, -1, -1}, {107, -1, -1, -1, -1}, {108, -1, -1, -1, -1}, {109, -1, -1, -1, -1},
         {110, -1, -1, -1, -1}, {111, -1, -1, -1, -1}, {112, -1, -1, -1, -1}, {113, -1, -1, -1, -1},
         {114, -1, -1, -1, -1}, {115, -1, -1, -1, -1}, {116, -1, -1, -1, -1}, {117, -1, -1, -1, -1},
         {118, -1, -1, -1, -1}},
    };

    static i32 group_count(i32 group) {
        if (group < 0 || group >= kGroups) return 0;
        return kGroupCounts[group];
    }

    // 写入最多 5 个 id 到 out（不足填 -2，对齐 0x4560b0 初值）
    static void fill_slot_ids(i32 group, i32 slot, i32 out[kIdsPerSlot]) {
        for (int i = 0; i < kIdsPerSlot; ++i) out[i] = -2;
        if (group < 0 || group >= kGroups || slot < 0 || slot >= kSlots) return;
        for (int i = 0; i < kIdsPerSlot; ++i) {
            const i32 id = kIds[group][slot][i];
            out[i] = id;
            if (id < 0) break;
        }
    }

    // 0x456060：全机体合计块 attempts[0]（+0x155c0 起）非 0 则解锁
    static bool unlocked(i32 group, i32 slot, const ScoreFile& scores) {
        i32 ids[kIdsPerSlot];
        fill_slot_ids(group, slot, ids);
        for (i32 id : ids) {
            if (id < 0) break;
            if (id >= ScoreFile::kSpellRecords) continue;
            // 合计块 kAllShotsBlock；attempts[0] ≡ 文件 +0x155c0 + id×0x9c
            if (scores.spells[ScoreFile::kAllShotsBlock][id].attempts[0] != 0) return true;
        }
        return false;
    }
};

}  // namespace th16
