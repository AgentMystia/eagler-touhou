// 符卡表现层（Stage1 范围）：原版符卡管理器对象（DAT_004c2178，0xc0 字节）
// 的开场/结算链路移植。
//
// 原版证据索引：
//   00413270.c（0x41e473 ECL 522/528/537-539 → 符卡开始）
//     :34      名字拷入 +0x38；+0x78 = 符卡号
//     :42      +0x7c = (旧值 & 0xffffff67) | 3 —— bit0 活动位 + bit1 取得位
//     :76-80   bit5（无计时符卡）按 DAT_004c2170+0x40
//     :82-105  三枚展示 VM（+0x14/+0x18/+0x1c）+ 名字文本（0x46e970 白）+
//              开场声 0x21（0x460bc0）
//     :158-186 背景枚 VM（+0x20）：enemy mgr bank 表（DAT_004c2188+0x80+idx*4）
//              ——st01 合并 bank 的 cardbg 脚本族；位置 = boss 世界位
//              （+0xac/+0xb0/+0xb4 快照）；时长参数入子 VM 变量（0xb/0xc 槽）
//     :224-229 bonus：+0x80 = +0x84 = (stage+difficulty)*1000000，
//              +0x84 封顶 99999999；+0x88 = 时长（秒）
//   00413a60.c（ECL 523 spellEnd → 0x413270 尾族）
//     :15-27   展示/背景 VM 回收（0x46fc30×3）+ 活动位清
//     :33-45   取得位（bit1）时：score += bonus/10（DAT_004be7c0 += +0x80/10，
//              封顶 999999999）+ 取得声 0x2e；bit0x80（最终符）声 0x45
//   00412ad0/0x413cc0（+8/+0xc 显隐）、00412b00（VM 清理族）
//
// 缺口（报主会话）：
//   - bonus 随时间衰减写点未定位（原版 +0x84 递减）；本类保初值不衰减
//   - 符卡名/「符卡取得 Bonus」文本位置未取证（0x46e970 经 ascii 管理器
//     当前状态位绘制）；文本面由 GameHudDraw 后续接入
//   - 取得位破坏点（死亡/炸弹清 bit1）原版写点未定位；宿主经 break_capture()
//     显式通知
#pragma once

#include "AnmManager.hpp"
#include "Types.hpp"

#include <functional>
#include <string>

namespace th13 {

class SpellPresentation {
public:
    // st01 合并 stage bank：enemy.anm（139 脚本）+ st01enm.anm（19 脚本）；
    // cardbg0 根 = st01enm 局部 11（spawn 12/13/14 子条带，dump 实证）。
    static constexpr i32 kStage1CardBgScript = 139 + 11;

    struct Banks {
        i32 stage = 4;  // Application kStageBank
    };
    explicit SpellPresentation(AnmManager& animations) : SpellPresentation(animations, Banks{}) {}
    explicit SpellPresentation(AnmManager& animations, Banks banks)
        : animations_(animations), banks_(banks) {}

    // 0x413270：符卡开始（id/名字/时长秒）。sound = 0x460bc0 同位回调。
    bool begin(i32 spell_id, const std::string& name, i32 duration_seconds,
               i32 stage_number, i32 difficulty, const float boss_position[3],
               const std::function<bool(i32)>& sound);
    // 0x413a60：符卡结束。captured = bit1 仍置（未破坏）。score 服务把
    // bonus/10 计入对局分（0x413a60:33-45 的 +0x80/10 同式）。
    bool end(bool captured, bool last_spell, const std::function<bool(i32)>& sound,
             const std::function<void(i32)>& add_score);
    // 取得位破坏（死亡/炸弹路径；原版写点未定位，宿主显式通知）。
    void break_capture() { flags_ &= ~0x2u; }

    void release();  // 场景卸载全回收

    bool active() const noexcept { return (flags_ & 0x1u) != 0; }
    bool captured() const noexcept { return (flags_ & 0x2u) != 0; }
    i32 bonus_base() const noexcept { return bonus_base_; }    // +0x80
    i32 bonus_display() const noexcept { return bonus_cap_; }  // +0x84
    i32 duration_seconds() const noexcept { return duration_; }  // +0x88
    i32 spell_id() const noexcept { return spell_id_; }
    const std::string& name() const noexcept { return name_; }
    u32 background_handle() const noexcept { return background_; }

    std::string error;

private:
    AnmManager& animations_;
    Banks banks_;
    u32 flags_ = 0;      // +0x7c：bit0 活动、bit1 取得、bit5 无计时
    i32 spell_id_ = -1;  // +0x78
    std::string name_;   // +0x38
    i32 bonus_base_ = 0, bonus_cap_ = 0;  // +0x80/+0x84
    i32 duration_ = 0;                    // +0x88
    u32 background_ = 0;                  // +0x20 背景枚 VM
    float anchor_[3] = {0, 0, 0};         // +0xac.. 快照位
};

}  // namespace th13
