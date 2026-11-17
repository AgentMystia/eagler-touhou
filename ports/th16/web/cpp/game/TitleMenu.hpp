// th16 标题主菜单——原版 Title 对象（DAT_004a6f20）的 0x44b5f0（菜单子状态 +0x20 的 case 0/1/2/4）。
//
// Title 对象偏移：
//   +0x18 画面状态（0x44a560）  +0x1c 上一画面状态  +0x20 菜单子状态（0x44a5e0）
//   +0x24 MenuCursor（+0x2c 项数=10，+0xf4 wrap=1）
//   +0x2ac Timer（0x44af80 末尾每帧 +1）  +0x2c0 logo VM（title.anm script0）
//   +0x444 装饰 0x61  +0x454 装饰 0x65  +0x7f0 title_v.anm script0
//
// 子状态时序（帧 = 标题更新链调用次数）：
//   case 0  第 0 帧：生成装饰（interrupt_now 2），Timer=0x78，落入 case 1
//   case 1  Timer==0x78 生成 logo/title_v，子项 3..12/13..22 按光标前后给 0x1e/0x1f；Timer>0x82（第 11 帧）进 case 2，
//           interrupt_now(logo,3) + interrupt(logo, cursor+0x11)
//   case 2  上下（pressed|repeat）移动，变化时 SFX 10 + interrupt_now(logo,3) + interrupt(logo,cursor+7) + 子项刷新；
//           取消 0x102：光标≠9 → SFX 9、光标=9；光标==9 → 当作确定；确定 0x80001：interrupt(logo,6)，按项发 SFX 7/9 并进 case 4
//   case 4  Timer>0x13 后进入对应画面（0x44a560：0/1/2→5，3→0x11，4→0xb，5→0xa，6→0xd，7 Option→3，8 Manual→0x10，9→2）
// 菜单项顺序（title.anm 子脚本 3..12）：Game Start / Extra Start / Practice / Spell Practice / Replay /
//   Player Data / Music Room / Option / Manual / Quit
#pragma once
#include "AnmManager.hpp"
#include "Input.hpp"
#include "MenuCursor.hpp"
#include "Types.hpp"

#include <vector>

namespace th16 {

struct TitleMenu {
    static constexpr i32 kItemCount = 10;
    static constexpr i32 kIntroTimer = 0x78;
    static constexpr i32 kIntroDone = 0x82;
    static constexpr i32 kConfirmHold = 0x13;

    enum class SubState : i32 { Decor = 0, Intro = 1, Main = 2, Confirmed = 4 };

    // 0x44a560 的目标画面状态
    enum class Screen : i32 {
        None = -1, Quit = 2, Option = 3 /*0x44c570*/, Difficulty = 5, Score = 0xa, Replay = 0xb,
        MusicRoom = 0xd, Manual = 0x10 /*0x4545a0*/, SpellPractice = 0x11,
    };

    struct Event { const char* kind; i32 value; };

    SubState sub = SubState::Decor;
    MenuCursor cursor;             // +0x24
    i32 prev_cursor = 0;           // +0x28
    i32 timer = 0;                 // +0x2b0（current）
    u32 logo_vm = 0;               // +0x2c0
    u32 decor_61 = 0;              // +0x444
    u32 decor_65 = 0;              // +0x454
    u32 title_v_vm = 0;            // +0x7f0
    AnmFile* title_file = nullptr; // +0x10
    AnmFile* title_v_file = nullptr;  // +0x14
    bool has_clear_data = false;   // 0x44a850() != 0：Extra 可选
    i32 selected = -1;
    Screen next_screen = Screen::None;
    std::vector<Event> events;     // SFX / 画面切换，供 harness 断言

    void activate(AnmFile* title, AnmFile* title_v) {
        *this = TitleMenu{};
        title_file = title;
        title_v_file = title_v;
        sub = SubState::Decor;
    }

    bool live() const { return title_file != nullptr; }

    // 0x44a700：logo 子树里 template_script == script 的 VM 置中断并立即 run
    void item_interrupt_now(AnmManager& anm, i32 script, i32 value) {
        AnmVm* logo = anm.find(logo_vm);
        if (logo == nullptr) { logo_vm = 0; return; }
        AnmVm* item = AnmManager::find_child(*logo, script, 0);
        if (item == nullptr) return;
        item->pending_interrupt = value;
        anm.run(*item);
    }

    // 0x44a680：同上，不 run
    void item_interrupt(AnmManager& anm, i32 script, i32 value) {
        AnmVm* logo = anm.find(logo_vm);
        if (logo == nullptr) { logo_vm = 0; return; }
        AnmVm* item = AnmManager::find_child(*logo, script, 0);
        if (item == nullptr) return;
        item->pending_interrupt = value;
    }

    // 光标前的项（3+i / 13+i）→ 0x1e，光标后的项 → 0x1f；无通关数据时项 1（脚本 4/0xe）→ 0x1d
    void refresh_items(AnmManager& anm) {
        i32 i = 0;
        for (; i < cursor.index; ++i) {
            item_interrupt_now(anm, i + 3, 0x1e);
            item_interrupt_now(anm, i + 0xd, 0x1e);
        }
        if (i + 1 < kItemCount) {
            for (i += 0xe; i < 0x17; ++i) {
                item_interrupt_now(anm, i - 10, 0x1f);
                item_interrupt_now(anm, i, 0x1f);
            }
        }
        if (!has_clear_data) {
            item_interrupt(anm, 4, 0x1d);
            item_interrupt(anm, 0xe, 0x1d);
        }
    }

    void set_sub(SubState s) {
        sub = s;
        timer = 0;  // 0x44a5e0：Timer 清零
    }

    void sfx(i32 id) { events.push_back({"sfx", id}); }

    // 0x44b5f0；每帧由标题更新链（优先级 6）调用，返回后 0x44af80 末尾 Timer+1
    void update(AnmManager& anm, const InputState& input) {
        if (title_file == nullptr) return;
        switch (sub) {
        case SubState::Decor:
            cursor.count = kItemCount;
            cursor.wrap = true;
            if (!has_clear_data) cursor.disable(1);
            set_sub(SubState::Intro);
            if (anm.find(decor_61) == nullptr) {
                decor_61 = anm.spawn(*title_file, 0x61, -1);
                anm.interrupt_now(decor_61, 2);
            }
            if (anm.find(decor_65) == nullptr) {
                decor_65 = anm.spawn(*title_file, 0x65, -1);
                anm.interrupt_now(decor_65, 2);
            }
            anm.interrupt_now(decor_61, 2);  // +0x1c != 3（不是从 Option 返回）
            timer = kIntroTimer;             // 0x406490(&Timer, 0x78)
            [[fallthrough]];
        case SubState::Intro:
            if (timer == kIntroTimer) {
                logo_vm = anm.spawn(*title_file, 0, -1);
                if (anm.find(title_v_vm) == nullptr) {
                    title_v_vm = anm.spawn(*title_v_file, 0, -1);
                }
                refresh_items(anm);
            }
            if (timer > kIntroDone) {
                sub = SubState::Main;
                timer = 0;
                anm.interrupt_now(logo_vm, 3);
                anm.interrupt(logo_vm, i32(i16(cursor.index + 0x11)));
                refresh_items(anm);
            }
            break;
        case SubState::Main: {
            prev_cursor = cursor.index;
            const u32 edge = input.pressed | input.repeat;
            if ((edge & InputState::kUp) != 0) cursor.move(-1);
            if ((edge & InputState::kDown) != 0) cursor.move(1);
            if (prev_cursor != cursor.index) {
                sfx(10);
                anm.interrupt_now(logo_vm, 3);
                anm.interrupt(logo_vm, i32(i16(cursor.index + 7)));
                refresh_items(anm);
            }
            bool confirm = (input.pressed & InputState::kConfirmMask) != 0;
            if ((input.pressed & InputState::kCancelMask) != 0) {
                if (cursor.index == 9) {
                    confirm = true;  // switchD_0044c258_caseD_9：在 Quit 上取消 = 确定退出
                } else {
                    sfx(9);
                    cursor.set(9);
                    anm.interrupt_now(logo_vm, 3);
                    anm.interrupt(logo_vm, i32(i16(cursor.index + 7)));
                    refresh_items(anm);
                }
            }
            if (confirm) {
                anm.interrupt(logo_vm, 6);
                selected = cursor.index;
                switch (selected) {
                case 7:
                    sfx(7);
                    break;
                case 9:
                    sfx(9);
                    break;
                default:
                    sfx(7);
                    anm.interrupt(decor_65, 1);
                    decor_65 = 0;
                    anm.interrupt(title_v_vm, 1);
                    break;
                }
                set_sub(SubState::Confirmed);
            }
            break;
        }
        case SubState::Confirmed:
            if (timer > kConfirmHold) {
                // 0x44b5f0 case 4：按项进入画面；DAT_004a5790 = 难度槽等由各画面自行处理
                static constexpr Screen kTargets[kItemCount] = {
                    Screen::Difficulty, Screen::Difficulty, Screen::Difficulty, Screen::SpellPractice,
                    Screen::Replay, Screen::Score, Screen::MusicRoom, Screen::Option,
                    Screen::Manual, Screen::Quit,
                };
                if (selected != 7 && selected != 9) anm.interrupt_now(decor_61, 3);
                next_screen = kTargets[selected];
                cursor.push();  // 0x402de0
                events.push_back({"screen", i32(next_screen)});
                title_file = nullptr;  // 主菜单结束，等待后续画面接管
            }
            break;
        }
        ++timer;  // 0x44af80 末尾
    }
};

}  // namespace th16
