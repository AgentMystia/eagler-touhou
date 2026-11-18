// th16 Spellcard——原版 0x4178a0 / 0x417f00 / 0x417930 / 0x4182f0（DAT_004a6db0）。
#include "Spellcard.hpp"

#include <cstring>

#include "AnmManager.hpp"
#include "Enemy.hpp"
#include "GameGlobals.hpp"
#include "Player.hpp"
#include "Stage.hpp"
#include "StageTable.hpp"

namespace th16 {

namespace {

// 0x417f00 内联：难度 → 符卡基础奖励（× 关卡号）
constexpr i32 kBonusByDifficulty[5] = {500000, 1000000, 1500000, 2000000, 1000000};

}  // namespace

// 0x4178a0
void Spellcard::reset() {
    // 0x4178a0：整块清零（`*_Dst |= 2` 落在 +0 的链表字段而非 +0x78 标志）；0x417700 末尾 Timer::reset（previous = -1）
    for (u32& id : vm_ids) id = 0;
    timer = Timer{};
    timer.reset();
    std::memset(name, 0, sizeof(name));
    id = 0;
    flags = 0;
    bonus = bonus_initial = time_limit = 0;
    capture_count = 0;
    state = 0;
    boss_pos = Vec3{};
    sfx_queue.clear();
    gui_events.clear();
    gui_timer_events.clear();
}

// 0x417f00 / 0x417930 内联：EnemyManager+0x48 槽 0 的 Boss；链表找不到时原版落在最后遍历到的敌机上
Vec3 Spellcard::boss_position() const {
    if (enemies == nullptr || enemies->boss_ids[0] == 0) return Vec3{};
    const Enemy* last = nullptr;
    for (const auto& e : enemies->enemies) {
        if (!e) continue;
        last = e.get();
        if (e->id == enemies->boss_ids[0]) break;
    }
    return last != nullptr ? last->pos : Vec3{};
}

// 0x417f00
void Spellcard::start(i32 spell_id, const char* spell_name, i32 limit, i32 anm_index) {
    timer.reset();
    id = spell_id;
    std::strncpy(name, spell_name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    flags = (flags & ~(kSurvival | kNoTimeout | kTimedOut)) | (kActive | kBonusAvailable);
    // 成绩文件（DAT_004a6f0c）符卡挑战计数与名字登记：ScoreFile 接入时补（非回放模式写入）
    gui_timer_events.push_back(2);  // 0x4175d0：Gui+0x94 / +0x98 计时 VM 中断 2
    state = 1;
    flags &= ~(kBombUsed | kTiming);

    AnmManager* anm = animations;
    AnmFile* ascii = anm != nullptr ? anm->file(AnmManager::kAsciiSlot) : nullptr;
    AnmFile* text = anm != nullptr ? anm->file(0) : nullptr;
    AnmFile* effect = anm != nullptr ? anm->file(8) : nullptr;
    if (anm != nullptr) {
        vm_ids[1] = ascii != nullptr ? anm->spawn(*ascii, 0, -1) : 0;
        vm_ids[2] = text != nullptr ? anm->spawn(*text, 2, -1) : 0;
        vm_ids[3] = ascii != nullptr ? anm->spawn(*ascii, 1, -1) : 0;
        // 0x46dab0：名字文本写入 text.anm VM 的纹理（渲染侧接入时补）
        if (anm->find(vm_ids[2]) == nullptr) vm_ids[2] = 0;
    }
    sfx_queue.push_back(0x21);

    // effect.anm 0xd 倒计时：entity_offset = Boss 坐标，子 VM（脚本 0xb / 0xc）的 $I2 = 时限
    boss_pos = boss_position();
    vm_ids[4] = 0;
    if (anm != nullptr && effect != nullptr) {
        vm_ids[4] = anm->spawn(*effect, 0xd, -1);
        if (AnmVm* vm = anm->find(vm_ids[4])) {
            vm->entity_offset = boss_pos;
            if (AnmVm* child = AnmManager::find_child(*vm, 0xb, 0)) child->int_vars[2] = limit;
            if (AnmVm* child = AnmManager::find_child(*vm, 0xc, 0)) child->int_vars[2] = limit;
        } else {
            vm_ids[4] = 0;
        }
    }
    time_limit = limit;

    const i32 difficulty = globals != nullptr ? globals->difficulty : 1;
    const i32 stage_number = globals != nullptr ? globals->stage : 1;
    i32 value = kBonusByDifficulty[(difficulty < 0 || difficulty > 4) ? 4 : difficulty] * stage_number;
    bonus = value;
    if (value > 999999999) value = 999999999;
    bonus_initial = value;
    if (anm != nullptr && effect != nullptr) anm->spawn(*effect, 0x14, -1);

    // 关卡表 Boss 块：spell_id < 0x2b（道中 Boss 段）且块 1 有背景 → 块 1，否则块 0
    if (entry != nullptr) {
        const i32 current_spell = globals != nullptr ? globals->spell_id : 0;
        const int block = (current_spell < 0x2b && entry->boss_anm[1].background_slot != -1) ? 1 : 0;
        const StageBossAnm& bg = entry->boss_anm[block];
        vm_ids[0] = 0;
        if (anm != nullptr && enemies != nullptr) {
            if (AnmFile* file = enemies->anm_file(bg.background_slot)) vm_ids[0] = anm->spawn(*file, bg.background_script, -1);
        }
        flags = (flags & ~kStageBackground) | (bg.background_keeps_stage != 0 ? kStageBackground : 0u);
        if (anm_index >= 0 && anm_index < 3) {
            const StageBossAnm& cutin = entry->boss_anm[anm_index];
            if (cutin.cutin_slot != -1 && anm != nullptr && enemies != nullptr) {
                if (AnmFile* file = enemies->anm_file(cutin.cutin_slot)) anm->spawn(*file, cutin.cutin_script, -1);
            }
        }
    }
}

// 0x417930
void Spellcard::update(float game_speed) {
    if ((flags & kActive) == 0) return;
    ++state;
    if (timer.current > 0x3b && (flags & kStageBackground) == 0 && stage != nullptr) stage->flags &= ~1u;
    if (timer.current > 299 && (flags & kSurvival) == 0) {
        bonus = ((bonus - (bonus_initial - bonus_initial / 3) / (time_limit - 300)) / 10) * 10;
    }
    timer.tick(game_speed);

    // 名牌翻转：自机 y < 96 → 名牌下移（中断 3），> 128 → 回到上方（中断 2）；kFlipped 时阈值 352 / 320
    if (timer.current > 0x77 && player != nullptr && animations != nullptr) {
        const float y = player->pos.y;
        const bool flipped = (flags & kFlipped) != 0;
        if ((flags & kNameLowered) == 0) {
            if ((!flipped && y < 96.0f) || (flipped && y > 352.0f)) {
                for (int i = 1; i <= 3; ++i) animations->interrupt(vm_ids[i], 3);
                flags |= kNameLowered;
            }
        } else if ((!flipped && y > 128.0f) || (flipped && y < 320.0f)) {
            for (int i = 1; i <= 3; ++i) animations->interrupt(vm_ids[i], 2);
            flags &= ~kNameLowered;
        }
    }

    const Vec3 target = boss_position();
    boss_pos.x = (target.x - boss_pos.x) * 0.05f + boss_pos.x;
    boss_pos.y = (target.y - boss_pos.y) * 0.05f + boss_pos.y;
    boss_pos.z = (target.z - boss_pos.z) * 0.05f + boss_pos.z;
    if (animations != nullptr) {
        if (AnmVm* vm = animations->find(vm_ids[4])) vm->entity_offset = boss_pos;
    }
    if ((flags & kBombUsed) != 0 && (enemies == nullptr || !enemies->bomb_active)) flags &= ~kBombUsed;
}

// 0x4182f0
void Spellcard::end() {
    if ((flags & kActive) == 0) return;
    if (stage != nullptr) stage->flags |= 1u;
    if (animations != nullptr) {
        for (int i = 1; i <= 3; ++i) animations->interrupt(vm_ids[i], 1);
    }
    flags &= ~kActive;
    if (animations != nullptr) animations->kill(vm_ids[0]);
    vm_ids[0] = 0;
    flags &= ~kBombUsed;
    gui_timer_events.push_back(3);  // 0x417650
    if (animations != nullptr) animations->kill(vm_ids[4]);
    vm_ids[4] = 0;
    if ((flags & kBonusAvailable) == 0) {
        gui_events.push_back({0, 1});
    } else {
        if (globals != nullptr) {
            // 0x43e080：每次 extend 另有 Gui 0x42c280 残机显示、音效 0x11、0x42bcf0(0, 4)
            const i32 extends = globals->add_score(bonus);
            for (i32 i = 0; i < extends; ++i) {
                sfx_queue.push_back(0x11);
                gui_events.push_back({0, 4});
            }
        }
        gui_events.push_back({bonus, 0});
        // 成绩文件符卡取得计数：ScoreFile 接入时补
        sfx_queue.push_back(0x2e);
    }
    if ((flags & kTimedOut) != 0) sfx_queue.push_back(0x45);
}

}  // namespace th16
