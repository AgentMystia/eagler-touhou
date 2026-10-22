#include "TitleSelectionMenu.hpp"

namespace th13 {

bool TitleDifficultyMenu::check(bool ok) {
    if (!ok && error.empty()) error = state.menu.error.empty() ? visuals.error : state.menu.error;
    return ok;
}

bool TitleDifficultyMenu::sound(i32 id) {
    if (play_sound && play_sound(id)) return true;
    error = "Title difficulty sound service unavailable";
    return false;
}

// 大行条 + 立绘（蓝图 §1.7 存疑项：select01 大行条归属按 golden 难度帧行位
// (32,288+48k) 最优猜测落码；立绘见于 golden 难度帧右列。均待 oracle 窗口比对）。
bool TitleDifficultyMenu::build_rows() {
    for (i32 k = 0; k < 4; k++) {
        const i32 script = resources.rank_row_base + k;  // 101-104：EASY..LUNATIC
        if (!check(visuals.create_at(script, Vec3{0, 280.f + 48.f * k, 0}))) return false;
    }
    if (!check(visuals.create_at(resources.rank_row_quit, Vec3{0, 280.f + 48.f * 4, 0})))
        return false;
    // Extra 行条（rank_row_extra=105）在无存档（Extra 未解锁）下不建——
    // golden 难度帧第 5 行即「やめる」，Extra 解锁换行机制待锚定（蓝图 §1.3）。
    // 立绘：preferred 变更后重访时旧槽一并清（四槽全 retire 防叠影）。
    for (i32 c = 0; c < 4; c++) visuals.retire(resources.portrait_base + c);
    const i32 portrait = resources.portrait_base + (settings.preferred_character & 3);
    if (!check(visuals.create(portrait))) return false;
    return check(visuals.interrupt(portrait, 7 + (settings.preferred_character & 3), true));
}

bool TitleDifficultyMenu::retire_rows() {
    bool ok = true;
    for (i32 k = 0; k < 4; k++) ok = check(visuals.retire(resources.rank_row_base + k)) && ok;
    ok = check(visuals.retire(resources.rank_row_quit)) && ok;
    for (i32 c = 0; c < 4; c++) ok = check(visuals.retire(resources.portrait_base + c)) && ok;
    return ok;
}

void TitleDifficultyMenu::choose_character() noexcept {
    state.change_screen(TitleScreen::Character);
    settings.configured_difficulty = state.menu.cursor;
    progress.difficulty = state.menu.cursor;
    state.menu.push();
    state.menu.wrapping = true;
    state.menu.count = kTitleCharacterRows;
    // 角色行 1-4 = 霊夢/魔理沙/早苗/妖夢（推定行位，蓝图 §1.4）。
    state.menu.select(settings.preferred_character + kTitleCharacterFirst);
    progress.character = settings.preferred_character;
}

void TitleDifficultyMenu::return_main() {
    state.change_screen(TitleScreen::Main);
    state.menu.pop();
}

bool TitleDifficultyMenu::update(u32 pressed, u32 repeated) {
    const i32 root = resources.difficulty_root;
    switch (state.substate) {
    case 0:
        state.menu.wrapping = false;
        state.menu.count = 5;  // 家内安全/無病息災/交通安全/學業成就/やめる（dump script1）
        if (!check(visuals.retire(root)) || !check(visuals.create(root)))
            return false;
        // 入场：仅 interrupt(root,7+cursor)（名行+数字行同号联动；不广播 label 3，
        // 同主菜单 oracle 结论）。th15 的 cursor+13 标签算式在 th13 不存在（蓝图 §4.1）。
        if (!check(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 7, true)))
            return false;
        if (!build_rows()) return false;
        state.change_substate(1);
        [[fallthrough]];
    case 1:
        if (state.age.current > 6) state.change_substate(2);
        break;
    case 2:
        state.menu.previous = state.menu.cursor;
        if ((pressed | repeated) & 0x10) state.menu.move(-1);  // ↑=0x10
        if ((pressed | repeated) & 0x20) state.menu.move(1);   // ↓=0x20
        if (!state.menu.error.empty()) return check(false);
        if (state.menu.previous != state.menu.cursor &&
            (!sound(10) ||
             !check(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 7))))
            return false;
        if (pressed & 0x102) {  // Esc/X：回主菜单
            state.change_substate(4);
            return sound(9);
        }
        if (pressed & 0x80001) {  // Z/Enter
            if (state.menu.cursor == 4) {  // やめる行：等同取消
                state.change_substate(4);
                return sound(9);
            }
            if (!check(visuals.interrupt(root, 6))) return false;  // 名行闪灭销毁
            if (!retire_rows()) return false;                      // 大行条滑出
            state.change_substate(3);
            return sound(7);
        }
        break;
    case 3:
        if (state.age.current > 13) choose_character();
        break;
    case 4:
        if (state.age.current > 5) {
            if (!check(visuals.retire(root)) || !retire_rows()) return false;
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

// 129 根广播：interrupt(129,label) 同时命中立绘 125+c / 武器 135+c / 选择框
// 131+c 的同号标签（蓝图 §1.8：label 7+c 从右入、12+c 从左入、17+c 出左、
// 22+c 出右、3 中央淡出、1 销毁——方向配对为推定，待 oracle 校正）。
bool TitleCharacterMenu::enter_portrait(i32 character, i32 label) {
    return visual(visuals.interrupt(resources.char_menu_root, label + character, false));
}

void TitleCharacterMenu::return_difficulty() {
    visuals.retire(resources.char_menu_root);
    visuals.retire(resources.character_root);
    state.change_screen(TitleScreen::Difficulty);
    if (state.menu.cursor >= kTitleCharacterFirst && state.menu.cursor <= kTitleCharacterLast)
        settings.preferred_character = title_character_at(state.menu.cursor);
    state.menu.pop();
}

bool TitleCharacterMenu::update(u32 pressed, u32 repeated) {
    const i32 root = resources.character_root;
    switch (state.substate) {
    case 0: {
        state.menu.count = kTitleCharacterRows;
        // 名行在确认（label 6）/取消（label 1）后子 VM 已销毁：重入必重建
        // （retire 对旧根广播 label 1 后新根并存淡出，交叉过渡与难度屏同构）。
        if (!visual(visuals.retire(root)) || !visual(visuals.create(root))) return false;
        if (!visual(visuals.retire(resources.char_menu_root)) ||
            !visual(visuals.create(resources.char_menu_root)))
            return false;
        if (!visual(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 7, true)))
            return false;
        if (!enter_portrait(title_character_at(state.menu.cursor), 7)) return false;
        state.change_substate(1);
        [[fallthrough]];
    }
    case 1:
        if (state.age.current > 6) state.change_substate(2);
        break;
    case 2: {
        state.menu.previous = state.menu.cursor;
        const bool was_character = state.menu.cursor >= kTitleCharacterFirst &&
                                   state.menu.cursor <= kTitleCharacterLast;
        if ((pressed | repeated) & 0x40) {  // ←：上一行
            if (was_character)
                if (!enter_portrait(title_character_at(state.menu.previous), 22)) return false;
            state.menu.move(-1);
            if (!state.menu.error.empty()) return check(false, state.menu.error.c_str());
            const bool now_character = state.menu.cursor >= kTitleCharacterFirst &&
                                       state.menu.cursor <= kTitleCharacterLast;
            if (now_character)
                if (!enter_portrait(title_character_at(state.menu.cursor), 12)) return false;
            if (!visual(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 7))) return false;
        }
        if ((pressed | repeated) & 0x80) {  // →：下一行
            if (was_character)
                if (!enter_portrait(title_character_at(state.menu.previous), 17)) return false;
            state.menu.move(1);
            if (!state.menu.error.empty()) return check(false, state.menu.error.c_str());
            const bool now_character = state.menu.cursor >= kTitleCharacterFirst &&
                                       state.menu.cursor <= kTitleCharacterLast;
            if (now_character)
                if (!enter_portrait(title_character_at(state.menu.cursor), 7)) return false;
            if (!visual(visuals.interrupt(root, i32(i16(state.menu.cursor)) + 7))) return false;
        }
        if (pressed & 0x102) {  // Esc/X：回难度菜单
            state.change_substate(4);
            return check(host.sound(9), "Character cancel sound failed");
        }
        if (pressed & 0x80001) {  // Z/Enter
            if (state.menu.cursor == kTitleCharacterRows - 1) {  // やめる行
                state.change_substate(4);
                return check(host.sound(9), "Character cancel sound failed");
            }
            if (state.menu.cursor < kTitleCharacterFirst ||
                state.menu.cursor > kTitleCharacterLast) {
                // 0/5 行（EXTRA/LUNATIC 字条位）：语义待锚定，本轮确认音后不动作。
                return check(host.sound(7), "Character confirm sound failed");
            }
            // 确认：立绘/武器/选择框中央淡出（label 3）+ 名行闪灭销毁（label 6）。
            if (!enter_portrait(title_character_at(state.menu.cursor), 3)) return false;
            if (!visual(visuals.interrupt(root, 6))) return false;
            if (!check(host.sound(7), "Character confirm sound failed")) return false;
            state.change_substate(3);
        }
        break;
    }
    case 3:
        if (state.age.current == 10) {
            if (!check(host.begin_transition(state.start_effect),
                       "Game start transition unavailable") ||
                !check(animations.interrupt(state.start_effect, 7),
                       "Game start transition interrupt failed"))
                return false;
        }
        if (state.age.current > 39) {
            progress.character = settings.preferred_character =
                title_character_at(state.menu.cursor);
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
