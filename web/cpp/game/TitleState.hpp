// 标题场景状态（裁剪自 th15 TitleState：仅留 Main/Difficulty/Character/StartGame/Quit，
// Replay/MusicRoom/Manual/Records/PlayerData/Practice/Demo 不做，docs/plans/ui-base.md §三）。
// TitleScreen 数值暂沿 th15 世代枚举位；th13 supervisor（0x44e600 对应物）锚定后回填。
#pragma once

#include "MenuCursor.hpp"
#include "Timer.hpp"

namespace th13 {

// th13 TitleScreen 数值待 supervisor 反汇编锚定（0x44e600 对应物，蓝图 §五-8）：
// 现沿 th15 世代枚举位，Quit=2 与 StartGame=2 同值冲突是有意保留的现状，
// TitleFrame/ApplicationState 以 screen==Quit 区分（待锚后回填）。
enum class TitleScreen : i32 {
    Initialize = 0,
    Main = 1,
    Quit = 2,
    StartGame = 2,
    Difficulty = 6,
    Character = 7,
};

struct TitleSelectionSettings {
    i32 configured_difficulty = 1, saved_difficulty = 0, preferred_character = 0;
};

struct TitleState {
    MenuCursor menu;
    TitleScreen screen = TitleScreen::Main, previous_screen = TitleScreen::Initialize;
    i32 substate = 0;
    Timer age{0, 0, 0, 0, 0};
    u32 flags = 2;
    std::array<u32, 248> handles{};
    u32 version_bar = 0;  // 版本条（bank17 script0；原 portrait 字段误名，蓝图 §1.9）
    i32 return_reason = 1;
    u32 start_effect = 0;

    TitleState() { age.set(0); }

    void change_screen(TitleScreen next) noexcept {
        previous_screen = screen;
        screen = next;
        substate = 0;
        age.set(0);
    }

    void change_substate(i32 value) noexcept {
        substate = value;
        age.set(0);
    }
};

}  // namespace th13
