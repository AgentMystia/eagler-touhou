// 暂停状态（th15 PauseState 裁剪：仅 Inactive/Pause/GameOver，
// Results/成绩登录/录像保存延后，ui-base.md §三）。
#pragma once

#include "MenuCursor.hpp"
#include "Timer.hpp"

namespace th13 {

enum class PauseScreen : i32 { Inactive = 0, Pause = 1, GameOver = 2 };

struct PauseState {
    Timer age, selection_age;
    MenuCursor menu;
    PauseScreen screen = PauseScreen::Inactive, previous = PauseScreen::Inactive;
    i32 phase = 0;
    u32 flags = 0;
    u32 menu_animation = 0, snapshot_animation = 0;
    i32 front_bank = -1;
    float saved_rate = 1;

    PauseState() {
        age.set(0);
        selection_age.set(0);
        menu.wrapping = false;
    }

    void select(PauseScreen next) noexcept {
        previous = screen;
        screen = next;
        phase = 0;
        menu.disabled_count = 0;
        age.set(0);
        selection_age.set(0);
    }

    void select_phase(i32 next) noexcept {
        phase = next;
        age.set(0);
    }
};

}  // namespace th13
