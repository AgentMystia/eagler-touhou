#include "TitleSelectionMenu.hpp"

namespace th13 {

bool TitleDifficultyMenu::check(bool ok) {
    if (!ok && error.empty()) error = visuals.error.empty() ? state.menu.error : visuals.error;
    return ok;
}

bool TitleDifficultyMenu::sound(i32 id) {
    if (play_sound && play_sound(id)) return true;
    error = "Title difficulty sound service unavailable";
    return false;
}

void TitleDifficultyMenu::choose_character() noexcept {
    state.change_screen(TitleScreen::Character);
    settings.configured_difficulty = state.menu.cursor;
    progress.difficulty = state.menu.cursor;
    state.menu.push();
    state.menu.wrapping = true;
    state.menu.count = 4;
    state.menu.select(settings.preferred_character);
    progress.character = settings.preferred_character;
}

void TitleDifficultyMenu::return_main() {
    visuals.retire(resources.difficulty_root);
    state.change_screen(TitleScreen::Main);
    state.menu.pop();
}

bool TitleDifficultyMenu::update(u32 pressed, u32 repeated) {
    const i32 root = resources.difficulty_root;
    switch (state.substate) {
    case 0:
        if (!check(visuals.prompt())) return false;
        state.menu.wrapping = false;
        state.menu.count = 4;  // Easy/Normal/Hard/Lunatic；Extra 待通关节件解锁
        if (!check(visuals.retire(root)) || !check(visuals.create(root)) ||
            !check(visuals.interrupt(root, 3, true)) ||
            !check(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 13)))
            return false;
        state.change_substate(1);
        [[fallthrough]];
    case 1:
        if (state.age.current > 6) state.change_substate(2);
        break;
    case 2:
        state.menu.previous = state.menu.cursor;
        if ((pressed | repeated) & 0x50) state.menu.move(-1);
        if ((pressed | repeated) & 0xa0) state.menu.move(1);
        if (!state.menu.error.empty()) return check(false);
        if (state.menu.previous != state.menu.cursor &&
            (!sound(10) || !check(visuals.interrupt(root, 3, true)) ||
             !check(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 7))))
            return false;
        if (pressed & 0x102) {  // Esc/X：回主菜单
            state.change_substate(4);
            return sound(9) && check(visuals.retire(root));
        }
        if (pressed & 0x80001) {
            if (!check(visuals.interrupt(root, 6))) return false;
            state.change_substate(3);
            return sound(7);
        }
        break;
    case 3:
        if (state.age.current > 13) choose_character();
        break;
    case 4:
        if (state.age.current > 5) {
            if (!check(visuals.retire(root))) return false;
            return_main();
        }
        break;
    }
    return true;
}

bool TitleCharacterMenu::check(bool ok, const char* reason) {
    if (!ok && error.empty()) error = reason;
    return ok;
}

bool TitleCharacterMenu::visual(bool ok) {
    if (!ok && error.empty()) error = visuals.error;
    return ok;
}

bool TitleCharacterMenu::selected_children(i32 root) {
    return visual(visuals.child_interrupt(root, state.menu.cursor + 126, 6)) &&
           visual(visuals.child_interrupt(root, state.menu.cursor + 147, 6)) &&
           visual(visuals.child_interrupt(root, 80, 6)) &&
           visual(visuals.child_interrupt(root, 81, 6));
}

void TitleCharacterMenu::return_difficulty() {
    visuals.retire(resources.character_root);
    state.change_screen(TitleScreen::Difficulty);
    progress.character = state.menu.cursor;
    state.menu.pop();
    settings.preferred_character = progress.character;
}

bool TitleCharacterMenu::update(u32 pressed, u32 repeated) {
    const i32 root = resources.character_root;
    switch (state.substate) {
    case 0: {
        state.menu.count = 4;  // Reimu/Marisa/Sanae/妖梦（pl00–pl03）
        if (!animations.registry.find(state.handles[root]) &&
            (!visual(visuals.retire(root)) || !visual(visuals.create(root))))
            return false;
        if (!visual(visuals.interrupt(root, 3, true)) ||
            !visual(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 7)))
            return false;
        state.change_substate(1);
        [[fallthrough]];
    }
    case 1:
        if (state.age.current > 6) state.change_substate(2);
        break;
    case 2:
        state.menu.previous = state.menu.cursor;
        if ((pressed | repeated) & 0x40) {  // ←：上一个角色
            if (!visual(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 25, true))) return false;
            state.menu.move(-1);
            if (!state.menu.error.empty()) return check(false, state.menu.error.c_str());
            if (!visual(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 13))) return false;
        }
        if ((pressed | repeated) & 0x80) {  // →：下一个角色
            if (!visual(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 19, true))) return false;
            state.menu.move(1);
            if (!state.menu.error.empty()) return check(false, state.menu.error.c_str());
            if (!visual(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 7))) return false;
        }
        if (pressed & 0x102) {  // Esc/X：回难度菜单
            state.change_substate(4);
            return check(host.sound(9), "Character cancel sound failed");
        }
        if (pressed & 0x80001) {
            if (!selected_children(root)) return false;
            if (!check(host.sound(7), "Character confirm sound failed")) return false;
            state.change_substate(3);
        }
        break;
    case 3:
        if (state.age.current == 10) {
            if (!check(host.begin_transition(state.start_effect),
                       "Game start transition unavailable") ||
                !check(animations.interrupt(state.start_effect, 7),
                       "Game start transition interrupt failed"))
                return false;
        }
        if (state.age.current > 39) {
            progress.character = settings.preferred_character = state.menu.cursor;
            state.menu.push();
            state.change_screen(TitleScreen::StartGame);
            progress.stage = 1;
            if (!check(host.start_game(progress.stage), "Game scene destination failed")) return false;
        }
        break;
    case 4:
        if (state.age.current > 5) return_difficulty();
        break;
    }
    return true;
}

}  // namespace th13
