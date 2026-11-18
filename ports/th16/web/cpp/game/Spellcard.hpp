// th16 Spellcard——原版 DAT_004a6db0（0x4178a0 创建；0x417f00 开始 / 0x417930 每帧 / 0x4182f0 结束）。
// 本文件先给出数据布局与敌机侧共用的标志位；ANM / Gui 相关的开始、逐帧与结束逻辑见 Spellcard.cpp。
#pragma once
#include "AnmVm.hpp"
#include "Types.hpp"

namespace th16 {

struct Spellcard {
    // +0x78 标志位
    static constexpr u32 kActive = 0x1;             // 符卡进行中（敌机受击按 7 倍血量池折算，0x41a6d0）
    static constexpr u32 kBonusAvailable = 0x2;     // 尚可取得（被弹 / 超时清除）
    static constexpr u32 kNameLowered = 0x4;        // 0x417930：自机靠上时名牌移到下方（VM 中断 3 / 2）
    static constexpr u32 kSurvival = 0x8;           // op 542：耐久符卡（超时 = 取得，奖励不递减）
    static constexpr u32 kNoTimeout = 0x10;         // op 543
    static constexpr u32 kBombUsed = 0x20;          // 0x425010：结束前 60 帧内用过炸弹
    static constexpr u32 kTiming = 0x40;            // 0x417bc0：实时计时中
    static constexpr u32 kTimedOut = 0x80;          // 0x425010：计时中断已触发
    static constexpr u32 kDamageSuppressed = 0x100; // op 567：伤害 /30（与 kActive 同置时，0x41c330）
    static constexpr u32 kStageBackground = 0x200;  // 0x417f00：关卡表 +0x3c 位

    u32 vm_ids[5]{};        // +0xc..+0x1c：关卡背景 / 名牌 front.anm 0 / text 名字 / front.anm 1 / effect.anm 0xd
    Timer timer;            // +0x20：符卡计时
    char name[0x41]{};      // +0x33
    i32 id = -1;            // +0x74：符卡编号（含难度偏移）
    u32 flags = 0;          // +0x78
    i32 bonus = 0;          // +0x7c：当前取得奖励（每帧递减）
    i32 bonus_initial = 0;  // +0x80
    i32 time_limit = 0;     // +0x84：op 537–539 arg（秒）
    i32 state = 0;          // +0x8c
    Vec3 boss_pos;          // +0xa8：开始时 Boss 坐标（特效 0xd 的 entity_offset）
};

}  // namespace th16
