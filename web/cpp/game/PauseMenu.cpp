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

// front132: 120/121/122/154/123；front133: 124/125/126（槽 0/1/4）；
// front142: 137/138/139/140/141。槽号同时决定 interrupt 7+slot。
i32 PauseMenu::entry_script(i32 entry) const {
    if (state.screen == PauseScreen::GameOver) return 137 + entry;
    if (state.practice) return entry == 4 ? 126 : 124 + entry;
    constexpr i32 scripts[] = {120, 121, 122, 154, 123};
    return scripts[entry];
}

bool PauseMenu::configure_entries() {
    auto& menu = state.menu;
    menu.count = 5;
    menu.wrapping = true;
    menu.disabled_count = 0;
    // 录像保存与设置尚未接入；保留原版槽位并用脚本的 disabled 标签显示。
    for (i32 entry : {2, 3})
        if (!menu.disable(entry) ||
            ((!state.practice || state.screen == PauseScreen::GameOver) &&
             !child(entry_script(entry), 14))) return false;
    if (state.screen == PauseScreen::GameOver &&
        (!menu.disable(0) || !child(entry_script(0), 14))) return false;
    menu.select(state.screen == PauseScreen::GameOver ? 1 : 0);
    return true;
}

bool PauseMenu::close() {
    return interrupt(state.menu_animation, 1) &&
           interrupt(state.snapshot_animation, 1);
}

bool PauseMenu::update(u32 pressed, u32 repeated) {
    if (!error.empty()) return false;
    auto& menu = state.menu;
    const i32 age = state.age.current;
    auto phase = [&](i32 next) { state.select_phase(next); };
    switch (state.phase) {
    case 0:  // 入场延迟后展开菜单
        if (age <= 9) break;
        phase(6);
        if (!interrupt(state.menu_animation, 3, true) || !configure_entries()) return false;
        return
               interrupt(state.menu_animation, i32(i16(menu.cursor)) + 7);
    case 6:
        if (!navigate(pressed, repeated, 7)) return false;
        if (pressed & 0x80001) {
            if (!host.sound(7)) return false;
            state.selected_action = menu.cursor;
            if (menu.cursor == 0 || state.screen == PauseScreen::GameOver) {
                if (!close()) return false;
                phase(16);
            } else {
                if (!child(entry_script(menu.cursor), 6)) return false;
                phase(8);  // 原版 0043e780 case5：先闪烁20帧，再打开 Yes/No。
            }
            return true;
        }
        if ((pressed & 0x100) && state.screen == PauseScreen::Pause) {
            if (!close()) return false;
            state.selected_action = 0;
            phase(16);
        } else if ((pressed & 0x102) && state.screen == PauseScreen::GameOver) {
            // GameOver 不能用 Esc 恢复一个已经死亡的对局。
            menu.select(1);
            return interrupt(state.menu_animation, 8) && host.sound(9);
        }
        break;
    case 8:
        if (age < 20) break;
        if (!interrupt(state.menu_animation, 14, true)) return false;
        menu.count = 2;
        menu.disabled_count = 0;
        menu.select(1);  // 原版默认 No，不能误触重开/回标题。
        if (!interrupt(state.menu_animation, 16)) return false;
        phase(9);
        break;
    case 9:
        if (!navigate(pressed, repeated, 15)) return false;
        if (pressed & 0x80001) {
            if (!child(menu.cursor == 0 ? 130 : 131, 6) ||
                !host.sound(menu.cursor == 0 ? 7 : 9)) return false;
            phase(10);
        } else if (pressed & 0x102) {
            if (!host.sound(9)) return false;
            if (menu.cursor == 0) {
                menu.select(1);
                return interrupt(state.menu_animation, 16);
            }
            if (!child(131, 6)) return false;
            phase(10);
        }
        break;
    case 10:
        if (age < 20) break;
        if (menu.cursor == 0) {
            if (!close()) return false;
            phase(16);
        } else {
            const i32 selected = state.selected_action;
            if (!interrupt(state.menu_animation, 3, true) || !configure_entries()) return false;
            menu.select(selected);
            if (!interrupt(state.menu_animation, 7 + selected)) return false;
            phase(6);
        }
        break;
    case 16:  // 动画收场后执行所选动作
        if (age < 12) break;
        if (!check(host.finish_selection(state.selected_action), "Pause selected action failed"))
            return false;
        state.select(PauseScreen::Inactive);
        break;
    }
    return true;
}

}  // namespace th13
