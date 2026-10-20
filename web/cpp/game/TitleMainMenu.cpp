#include "TitleMainMenu.hpp"

namespace th13 {

bool TitleMainMenu::fail(const std::string& reason) {
    if (error.empty()) error = reason;
    return false;
}

bool TitleMainMenu::present(u32& handle) {
    if (animations.registry.find(handle)) return true;
    handle = 0;
    return false;
}

bool TitleMainMenu::create(i32 script) {
    state.handles[script] = animations.create(resources.title, script, -1, 0);
    return state.handles[script] || fail(animations.error);
}

bool TitleMainMenu::interrupt(i32 script, i32 label, bool immediate) {
    return animations.interrupt(state.handles[script], label, immediate) || fail(animations.error);
}

bool TitleMainMenu::child_interrupt(i32 script, i32 label, bool immediate) {
    auto* root = animations.registry.find(state.handles[resources.main_root]);
    if (!root) {
        state.handles[resources.main_root] = 0;
        return fail("Title menu root unavailable");
    }
    auto* child = animations.registry.find_child_script(*root, script, 0);
    if (!child) return fail("Title menu child unavailable");
    child->pending_interrupt = label;
    return !immediate || animations.tick_instance(*child) >= 0 || fail(animations.error);
}

bool TitleMainMenu::sound(i32 id) {
    return play_sound ? play_sound(id) || fail("Title sound request failed")
                      : fail("Title sound service unavailable");
}

// 高亮排布：选中项前的子脚本 30、其后的 31（th15 主菜单同构；子脚本号待锚定）。
bool TitleMainMenu::paint() {
    for (i32 i = 0; i < state.menu.cursor; i++)
        if (!child_interrupt(i + 3, 30, true) || !child_interrupt(i + 12, 30, true)) return false;
    for (i32 i = state.menu.cursor + 1; i < kTitleMainEntries; i++)
        if (!child_interrupt(i + 3, 31, true) || !child_interrupt(i + 12, 31, true)) return false;
    return true;
}

bool TitleMainMenu::select_animation() {
    return interrupt(resources.main_root, 3, true) &&
           interrupt(resources.main_root, i32(i16(state.menu.cursor)) + 7, false) && paint();
}

void TitleMainMenu::enter(i32 entry) {
    switch (TitleMainEntry(entry)) {
    case TitleMainEntry::StartGame:
        state.change_screen(TitleScreen::Difficulty);
        state.menu.push();
        settings.saved_difficulty = progress.difficulty;
        state.menu.count = 4;
        state.menu.select(settings.configured_difficulty);
        break;
    case TitleMainEntry::Replay:
    case TitleMainEntry::PlayerData:
    case TitleMainEntry::MusicRoom:
        break;  // 本轮禁用（菜单构建处 disable），不可达
    case TitleMainEntry::Quit:
        state.change_screen(TitleScreen::Quit);
        break;
    }
}

bool TitleMainMenu::update(u32 pressed, u32 repeated) {
    switch (state.substate) {
    case 0:
        state.menu.count = kTitleMainEntries;
        state.menu.wrapping = true;
        // Replay/PlayerData/MusicRoom 分期未做：禁用对应光标项。
        if (!state.menu.disable(1) || !state.menu.disable(2) || !state.menu.disable(3))
            return fail(state.menu.error);
        state.change_substate(1);
        if (!(state.flags & 2)) {
            // 首次进入：按键提示入场（白闪后停在提示幕，等待 age 120 的主菜单展开）。
            if (!present(state.handles[resources.main_prompt]) &&
                (!create(resources.main_prompt) || !interrupt(resources.main_prompt, 2, true)))
                return false;
            if (state.previous_screen != TitleScreen::Quit &&
                !interrupt(resources.main_prompt, 2, true))
                return false;
            state.age.set(120);
        } else {
            if (!create(resources.main_prompt)) return false;
            state.flags &= ~2u;
        }
        [[fallthrough]];
    case 1:
        if (state.age.current == 120) {
            if (!create(resources.main_root)) return false;
            if (!present(state.portrait)) {
                state.portrait = animations.create(resources.portrait, 0, -1, 0);
                if (!state.portrait) return fail(animations.error);
            }
            if (!paint()) return false;
        }
        if (state.age.current > 130) {
            state.change_substate(2);
            if (!select_animation()) return false;
        }
        break;
    case 2:
        state.menu.previous = state.menu.cursor;
        if ((pressed | repeated) & 16) state.menu.move(-1);
        if ((pressed | repeated) & 32) state.menu.move(1);
        if (!state.menu.error.empty()) return fail(state.menu.error);
        if (state.menu.previous != state.menu.cursor && (!sound(10) || !select_animation()))
            return false;
        if (pressed & 0x102) {  // Esc/X：直接进入退出分支
            if (state.menu.cursor == kTitleMainEntries - 1) {
                if (!sound(9)) return false;
                state.change_substate(4);
                return true;
            }
            if (!sound(9)) return false;
            state.menu.select(kTitleMainEntries - 1);
            if (!select_animation()) return false;
        }
        if (pressed & 0x80001) {  // Z/Enter：确认
            if (!interrupt(resources.main_root, 6, false)) return false;
            const i32 selected = state.menu.cursor;
            if (selected >= 0 && selected < kTitleMainEntries) {
                if (!sound(selected == kTitleMainEntries - 1 ? 9 : 7)) return false;
                if (selected != kTitleMainEntries - 1) {
                    if (!interrupt(resources.main_prompt, 1, false)) return false;
                    state.handles[resources.main_prompt] = 0;
                    if (!animations.interrupt(state.portrait, 1, false)) return fail(animations.error);
                }
                state.change_substate(4);
            }
        }
        break;
    case 4:
        if (state.age.current > 19) {
            if (!interrupt(resources.main_prompt, 3, true)) return false;
            enter(state.menu.cursor);
        }
        break;
    }
    return true;
}

}  // namespace th13
