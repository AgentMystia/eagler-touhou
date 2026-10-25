// SpellPresentation 实现：0x413270/0x413a60 的行为移植（证据行号见头注释）。
#include "SpellPresentation.hpp"

#include <algorithm>

namespace th13 {

bool SpellPresentation::begin(i32 spell_id, const std::string& name, i32 duration_seconds,
                              i32 stage_number, i32 difficulty, const float boss_position[3],
                              const std::function<bool(i32)>& sound) {
    if (active()) {
        // 原版 0x41e473 单线程序：前卡未结即开新卡 = 状态错误，显式失败。
        error = "Spell card started while previous card active";
        return false;
    }
    // 0x413270:34/42：名字、符卡号、活动位 + 取得位置位。
    spell_id_ = spell_id;
    name_ = name;
    flags_ = 0x3u;  // bit0 | bit1（旧值无关：上一卡已清）
    // 0x413270:224-229：bonus 基值 = (stage + difficulty) * 1000000，
    // 显示值封顶 99999999（DAT_004be81c + DAT_004be7c4 同式）。
    bonus_base_ = (stage_number + difficulty) * 1000000;
    bonus_cap_ = std::min(bonus_base_, 99999999);
    duration_ = duration_seconds;  // +0x88（ECL 537 duration 参）
    for (i32 i = 0; i < 3; i++) anchor_[i] = boss_position[i];
    // 0x413270:158-186：背景枚 VM（合并 stage bank 的 cardbg 族），
    // 位置 = boss 快照位（+0x574.. 直写）。子 VM 时长变量由脚本族内
    // ins_500 派生，不在此旁路。
    background_ = animations_.create(banks_.stage, kStage1CardBgScript, -1, 0);
    if (!background_) {
        error = "Spell card background animation unavailable: " + animations_.error;
        return false;
    }
    if (auto* vm = animations_.registry.find(background_)) {
        vm->visual.render_flags &= ~0xc000u;
        vm->visual.translation = {anchor_[0], anchor_[1], anchor_[2]};
    }
    // 0x413270:157：开场声 0x21。
    if (sound && !sound(0x21)) {
        error = "Spell card start sound service failed";
        return false;
    }
    return true;
}

bool SpellPresentation::end(bool captured, bool last_spell,
                            const std::function<bool(i32)>& sound,
                            const std::function<void(i32)>& add_score) {
    if (!active()) return true;  // 无卡：原版 0x413a60 头门（+0x7c bit0）
    // 0x413a60:15-27：展示/背景 VM 回收 + 活动位清。
    if (background_ && !animations_.retire(background_))
        error = "Spell card background retire failed";
    background_ = 0;
    const bool award = captured && (flags_ & 0x2u) != 0;  // bit1 未破坏（0x413a60:33）
    if (award) {
        // 0x413a60:34-36：score += bonus/10（封顶由宿主分数字段承担）。
        if (add_score) add_score(bonus_base_ / 10);
        // 0x413a60:60：取得声 0x2e。
        if (sound && !sound(0x2e)) error = "Spell card capture sound service failed";
        // 0x413a60:64-66：最终符（bit0x80）追加声 0x45。
        if (last_spell && sound && !sound(0x45))
            error = "Spell card last-spell sound service failed";
    }
    flags_ = 0;
    spell_id_ = -1;
    name_.clear();
    bonus_base_ = bonus_cap_ = 0;
    duration_ = 0;
    return error.empty();
}

void SpellPresentation::release() {
    if (background_ && !animations_.retire(background_))
        error = "Spell card background retire failed";
    background_ = 0;
    flags_ = 0;
}

}  // namespace th13
