#include "PauseMenu.hpp"

namespace th13 {

bool PauseMenu::check(bool ok, const char* reason) {
    if (!ok && error.empty()) error = reason;
    return ok;
}

bool PauseMenu::interrupt(u32 handle, i32 label, bool immediate) {
    return check(animations.interrupt(handle, label, immediate), "Pause animation interrupt failed");
}

bool PauseMenu::child(i32 script, i32 label) {
    auto* root = animations.registry.find(state.menu_animation);
    if (!root) {
        state.menu_animation = 0;
        return true;
    }
    auto* vm = animations.registry.find_child_script(*root, script, 0);
    return !vm || interrupt(animations.registry.handle(*vm), label);
}

bool PauseMenu::navigate(u32 pressed, u32 repeated, i32 label) {
    auto& menu = state.menu;
    menu.previous = menu.cursor;
    if ((pressed | repeated) & 0x10) menu.move(-1);
    if ((pressed | repeated) & 0x20) menu.move(1);
    if (!menu.error.empty()) return check(false, "Pause cursor movement failed");
    return menu.previous == menu.cursor ||
           (interrupt(state.menu_animation, i32(i16(menu.cursor)) + label) && host.sound(10));
}

bool PauseMenu::update(u32 pressed, u32 repeated) {
    if (!error.empty()) return false;
    auto& menu = state.menu;
    const i32 age = state.age.current;
    auto phase = [&](i32 next) { state.select_phase(next); };
    auto close = [&]() { return interrupt(state.menu_animation, 1); };
    switch (state.phase) {
    case 0:  // 入场延迟后展开菜单
        if (age <= 9) break;
        phase(6);
        menu.count = 3;  // 继续/重开/回标题
        menu.wrapping = true;
        if (state.screen == PauseScreen::GameOver) {
            if (!state.menu.disable(0)) return false;  // Game Over 无「继续」
            menu.select(1);
        } else {
            menu.select(0);
        }
        return interrupt(state.menu_animation, 3, true) &&
               interrupt(state.menu_animation, i32(i16(menu.cursor)) + 7);
    case 6:
        if (!navigate(pressed, repeated, 7)) return false;
        if (pressed & 0x80001) {
            if (!host.sound(7)) return false;
            if (menu.cursor != 0) {
                for (i32 script : {123, 132, 144})
                    if (!child(script, 6)) return false;  // 选中项子脚本收起
            }
            phase(16);
            state.age.set(0);
        }
        if (pressed & 0x100) {  // Esc：等同「继续」
            if (!close()) return false;
            menu.select(0);
            phase(16);
            state.age.set(0);
        }
        break;
    case 16:  // 动画收场后执行所选动作
        if (age < 12) break;
        if (!check(host.finish_selection(menu.cursor), "Pause selected action failed")) return false;
        state.select(PauseScreen::Inactive);
        break;
    }
    return true;
}

}  // namespace th13
