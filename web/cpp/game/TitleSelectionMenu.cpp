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

// [解剖轮2] 难度屏构成（difficulty-menu-anatomy.md §①；age0 = Z+21，难度
// 内容 f490=Z+21 起在场）：
//   create(123) → spawn 83,84,113..121（行条对 83/84、辅助 113/115/120、
//   徽记滚动组 114/116/121、右半 118、光标 119——全 parked 待中断/poke）；
//   create(101, {160,170}) → 脚本自带 -256→32 20f 滑入 = 固定徽记
//   (192,178) L26（解剖 §⑤「L26 一枚恒定」）；
//   徽记上滚 = 118 ← label 9（L23 y→-256 30f 插值，f490-f509 样本在中段 ✓）；
//   徽记下滚 = 114 ← supervisor 置位 poke（无对应标签序列，解剖 §⑤ f505/278、
//   f509/292 两点折线 6.67→3.5 px/帧）；
//   行条 83/84 落位动画（滑入期 title00a 双片对 → loading.png 四条 1px 错位对）
//   与粒子节奏逐帧几何以 title-draws.jsonl 对拍校准（本轮先建不驱动，待校）。
bool TitleDifficultyMenu::build_difficulty() {
    if (!check(visuals.create(resources.difficulty_tree))) return false;
    if (!check(visuals.create_at(resources.difficulty_badge, Vec3{160, 170, 0})))
        return false;
    auto* tree = animations.registry.find(state.handles[resources.difficulty_tree]);
    if (!tree) return check(false);
    // 光标 119：解剖 §② 实测层 L26（脚本 spawn 置 25，+1 现象同 §8.3）。
    if (auto* cursor = animations.registry.find_child_script(*tree, resources.difficulty_cursor, 0))
        cursor->visual.layer = 26;
    // 徽记上滚（118 label 9，逐子中断不广播——广播会牵动 119/120/121 同号体）。
    if (auto* up = animations.registry.find_child_script(*tree, 118, 0))
        animations.interrupt(animations.registry.handle(*up), 9, false);
    return true;
}

// [解剖轮2] 主菜单 Z+21 隐藏时间线（解剖 §③/⑥ + 顶点色）：
//   非选中灰条 9 枚隐藏（visible=false，VM 留池）；选中项白化（rgb→255、
//   alpha=204）续 -13/帧 衰减（advance_choreography 接力）至 Z+37 隐藏；
//   白条 9 枚（items 1-9 的 13+k = script14-22）闪现 8 帧（alpha 64→0，
//   -7/帧）后销毁——白条建后中和其 spawn 淡入插值（interpolators.alpha
//   .duration=0，poke 才不被逐帧覆写；引擎语义差异补偿见蓝图 §8.2）。
void TitleDifficultyMenu::finish_menu_hides() {
    auto* menu_root = animations.registry.find(state.handles[0]);
    if (!menu_root) return;
    for (i32 item = 0; item < kTitleMainEntries; item++) {
        auto* child = animations.registry.find_child_script(*menu_root, item + 3, 0);
        if (!child) continue;
        if (item == state.confirmed_entry) {
            child->visual.color = 0xccffffffu;  // 白化 @204（oracle f490 样本）
            continue;
        }
        child->visible = false;
        child->visual.flags &= ~1u;
    }
    for (i32 item = 1; item < kTitleMainEntries; item++) {
        const i32 script = 13 + item;  // 白条 14-22（items 1-9；0 号白条=选中灰条本体）
        if (!visuals.create(script)) continue;
        auto* white = animations.registry.find(state.handles[script]);
        if (!white) continue;
        white->interpolators.alpha.duration = 0;
        white->visual.color = 0x40ffffffu;  // alpha 64（oracle f490 L25 批本）
    }
}

// [解剖轮2] 神霊粒子：ascii bank2 script18 克隆（解剖「打字机不存在」纠偏）。
// 脚本体经 RNG 槽 10010-10012（AnmVariables 映射游戏 RNG 流）自算全屏随机
// 出生位与 300f 漂移目标（窗内不死灭 ✓ +300 自灭），逐帧几何在
// title-draws.jsonl——节奏（f495 起 17@f500→55@f599，≈1/3 帧）先按
// 「age5 突发 17 + 每 3 帧 1 枚」落码，出生次序/节奏待对拍校准。
void TitleDifficultyMenu::spawn_particles(i32 count) {
    for (i32 i = 0; i < count; i++)
        if (!animations.create(resources.ascii, resources.difficulty_particle, -1, 0)) {
            check(false);
            return;
        }
}

void TitleDifficultyMenu::advance_choreography() {
    const i32 f = frames;
    if (f < 0) return;
    // 光标 119 缓动序列（解剖 §②：f490-f508 十九帧 x 序列，y=8 恒定；子体
    // 相对根 123 (320,178) → translation = (x-320, 8-178)）。
    static const i32 kCursorEase[19] = {-202, -176, -152, -130, -110, -90, -72,
                                        -56,  -40,  -26,  -14,  -4,   6,   14,
                                        20,   26,   28,   30,   32};
    if (auto* tree = animations.registry.find(state.handles[resources.difficulty_tree])) {
        if (auto* cursor =
                animations.registry.find_child_script(*tree, resources.difficulty_cursor, 0)) {
            const i32 x = f < 19 ? kCursorEase[f] : 32;
            cursor->visual.translation = Vec3{float(x - 320), 8.f - 178.f, 0};
        }
        // 徽记下滚（114，解剖 §⑤：f505→278、f509→292 两点折线；y 相对根 178）。
        if (auto* down = animations.registry.find_child_script(*tree, 114, 0)) {
            const float y = f <= 15 ? 6.67f * f : 100.f + 3.5f * (f - 15);
            down->variables.position.y = y;
        }
    }
    // 白条衰减（64-7f/帧，f8 归零销毁；oracle f498=L25 批本消失）与选中白化
    // 衰减（204-13f，f16 归零隐藏；oracle f506=title01 全消）。
    if (f <= 8) {
        i32 alpha = 64 - 7 * f;
        if (alpha < 0) alpha = 0;
        for (i32 item = 1; item < kTitleMainEntries; item++) {
            const i32 script = 13 + item;
            auto* white = animations.registry.find(state.handles[script]);
            if (!white) continue;
            if (f == 8) {
                animations.retire(state.handles[script]);
                continue;
            }
            white->visual.color = u32(alpha) << 24 | 0xffffffu;
        }
    }
    if (auto* menu_root = animations.registry.find(state.handles[0])) {
        if (auto* selected =
                animations.registry.find_child_script(*menu_root, state.confirmed_entry + 3, 0)) {
            if (f < 16) {
                i32 alpha = 204 - 13 * f;
                if (alpha < 0) alpha = 0;
                selected->visual.color = u32(alpha) << 24 | 0xffffffu;
            } else if (f == 16) {
                selected->visible = false;
                selected->visual.flags &= ~1u;  // oracle f506：title01 全消
            }
        }
    }
    // 粒子节奏（age5 突发 17 + 每 3 帧 1 枚；总数 17+34=51 vs oracle 55，待校）。
    if (f == 5) spawn_particles(17);
    else if (f > 5 && f % 3 == 0) spawn_particles(1);
    // oracle f517（Z+48）：select00 右半自 L21 搬 L22（左半留 L21 = 层分裂）。
    // 真机由 script118 承接右半；移植侧直接搬既有 93 VM 层（几何等价，VM
    // 构成差异登记蓝图待裁）。
    if (f == 27) {
        if (auto* bg = animations.registry.find(state.handles[89]))
            if (auto* right = animations.registry.find_child_script(*bg, 93, 0))
                right->visual.layer = 22;
    }
}

void TitleDifficultyMenu::retire_difficulty() {
    visuals.retire(resources.difficulty_tree);
    visuals.retire(resources.difficulty_badge);
    // 背景右半层复位（难度期 f517 起在 L22；回主菜单推定复回 L21，待 oracle）。
    if (auto* bg = animations.registry.find(state.handles[89]))
        if (auto* right = animations.registry.find_child_script(*bg, 93, 0))
            right->visual.layer = 21;
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
    switch (state.substate) {
    case 0:
        state.menu.wrapping = false;
        state.menu.count = 5;  // 家内安全..やめる（行为层光标；难度屏构成按解剖）
        frames = 0;
        if (!build_difficulty()) return false;
        finish_menu_hides();
        state.change_substate(1);
        [[fallthrough]];
    case 1:
    case 2:
    case 3:
    case 4:
        advance_choreography();
        frames++;
        break;
    }
    switch (state.substate) {
    case 1:
        if (state.age.current > 6) state.change_substate(2);
        break;
    case 2:
        state.menu.previous = state.menu.cursor;
        if ((pressed | repeated) & 0x10) state.menu.move(-1);  // ↑=0x10
        if ((pressed | repeated) & 0x20) state.menu.move(1);   // ↓=0x20
        if (!state.menu.error.empty()) return check(false);
        if (state.menu.previous != state.menu.cursor) {
            // [解剖待校] 光标移动的真机构成（0x473210 移动器 + 徽记列更新）
            // 本轮仅音效，逐帧几何待窗对拍。
            if (!sound(10)) return false;
        }
        if (pressed & 0x102) {  // Esc/X：回主菜单
            state.change_substate(4);
            return sound(9);
        }
        if (pressed & 0x80001) {  // Z/Enter
            if (state.menu.cursor == 4) {  // やめる行：等同取消
                state.change_substate(4);
                return sound(9);
            }
            retire_difficulty();
            state.change_substate(3);
            return sound(7);
        }
        break;
    case 3:
        if (state.age.current > 13) choose_character();
        break;
    case 4:
        if (state.age.current > 5) {
            retire_difficulty();
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
