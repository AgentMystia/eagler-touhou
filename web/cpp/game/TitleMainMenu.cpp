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

bool TitleMainMenu::create_at(i32 script, const Vec3& translation) {
    // 逐实例平移覆盖（行条 y 步距 48px 由 golden 难度帧行位拟合，蓝图 §1.7）。
    state.handles[script] = animations.create(resources.title, script, -1, 2, translation);
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

// 全项入场：真机 boot 菜单全员可见（oracle 标题窗 L26 批次 10 字条 alpha 渐升、
// 色恒 192,192,192 实证），灰态逐项 interrupt(0,7+k)；禁用项 1 不入场、黑化。
// （th15 的 interrupt(root,3)+paint(30/31) 形状不搬：th13 灰态 label 3=淡出、
// 30/31=空操作，oracle 实测移动光标时旧项保持可见——不广播 label 3。）
bool TitleMainMenu::show_menu() {
    for (i32 k = 0; k < kTitleMainEntries; k++) {
        if (k == i32(TitleMainEntry::ExtraStart)) continue;
        if (!child_interrupt(k + 3, 7 + k, false)) return false;
    }
    dim_disabled();
    return true;
}

// 1 号项禁用观感：黑 + alpha160（oracle 顶点色 0xA0000000，自建帧起恒定）。
// 直接落子 VM 色（真机疑为 supervisor 逐 VM 置色，蓝图 §oracle 章「机制待锚」）。
void TitleMainMenu::dim_disabled() {
    auto* root = animations.registry.find(state.handles[resources.main_root]);
    if (!root) return;
    auto* child = animations.registry.find_child_script(
        *root, i32(TitleMainEntry::ExtraStart) + 3, 0);
    if (child) child->visual.color = 0xA0000000u;
}

void TitleMainMenu::enter(i32 entry) {
    switch (TitleMainEntry(entry)) {
    case TitleMainEntry::StartGame:
        state.flags |= 2u;  // 离开主菜单：返回时不再走 180 帧 intro
        state.change_screen(TitleScreen::Difficulty);
        state.menu.push();
        settings.saved_difficulty = progress.difficulty;
        state.menu.count = 5;  // 家内安全/無病息災/交通安全/學業成就/やめる
        state.menu.select(settings.configured_difficulty);
        break;
    case TitleMainEntry::Quit:
        state.change_screen(TitleScreen::Quit);
        break;
    default:
        // Replay/PlayerData/MusicRoom/Option/Manual 等入口：真机有画面，本轮不实现
        // （确认音已在按键处播放，不切屏；蓝图「未实现」清单登记）。
        break;
    }
}

bool TitleMainMenu::update(u32 pressed, u32 repeated) {
    switch (state.substate) {
    case 0: {
        state.menu.count = kTitleMainEntries;
        state.menu.wrapping = true;
        // Extra Start 无存档数据禁用（真机 scoreth13.dat 缺省行为，oracle ↓ 0→2 跳过）。
        if (!state.menu.disable(i32(TitleMainEntry::ExtraStart)))
            return fail(state.menu.error);
        // 真机 boot：无提示幕。标题任务使能帧（=g181）建背景+LOGO 根（oracle：
        // g181 起 select00 两半、g291 LOGO 翻转 = script94 内建 110f 延迟、
        // g350 森林 = script90/91 内建 170f）。
        if (!present(state.handles[resources.background_root])) {
            if (!create(resources.background_root)) return false;
        }
        const bool returning = (state.flags & 2u) != 0;
        if (!present(state.handles[resources.logo_root])) {
            const i32 logo = returning ? resources.logo_restore_root : resources.logo_root;
            if (!create(logo)) return false;
        }
        state.change_substate(1);
        if (returning) open_menu();
        [[fallthrough]];
    }
    case 1: {
        // 菜单 intro：boot 180 帧后建菜单（oracle g361：root0 + title01 十灰条
        // 入 L26 批次、title_v 版本条 30f 延迟淡入 → g390 可见）；返回路径立即建。
        const bool returning = (state.flags & 2u) != 0;
        const i32 menu_at = returning ? 0 : kTitleBootIntroFrames;
        if (!returning && state.age.current == menu_at && !open_menu()) return false;
        if (state.age.current >= menu_at + kTitleMenuReadyDelay) {
            // g372 起可交互（oracle 实测；帧数微差待 oracle 比对校正）。
            state.change_substate(2);
            if (!select_entry()) return false;
        }
        break;
    }
    case 2: {
        state.menu.previous = state.menu.cursor;
        if ((pressed | repeated) & 0x10) state.menu.move(-1);  // ↑=0x10（oracle 位布局）
        if ((pressed | repeated) & 0x20) state.menu.move(1);   // ↓=0x20
        if (!state.menu.error.empty()) return fail(state.menu.error);
        if (state.menu.previous != state.menu.cursor) {
            // 仅新项入场（oracle：旧项保持 255/192 静止、新项 alpha 自 ~0 快速渐升；
            // 不广播 label 3——灰态 label 3=淡出 5f 会遮没全菜单）。
            if (!sound(10) || !interrupt(resources.main_root,
                                         i32(i16(state.menu.cursor)) + 7, false))
                return false;
        }
        if (pressed & 0x102) {  // Esc/X：跳到末项或直接退出分支
            if (state.menu.cursor == kTitleMainEntries - 1) {
                if (!sound(9)) return false;
                state.change_substate(4);
                return true;
            }
            if (!sound(9)) return false;
            state.menu.select(kTitleMainEntries - 1);
            if (!interrupt(resources.main_root, i32(i16(state.menu.cursor)) + 7, false))
                return false;
        }
        if (pressed & 0x80001) {  // Z/Enter：确认
            const i32 selected = state.menu.cursor;
            if (selected < 0 || selected >= kTitleMainEntries) break;
            if (selected == i32(TitleMainEntry::StartGame)) {
                if (!sound(7)) return false;
                // 进难度屏（oracle 未覆盖该项确认序列）：全菜单淡出后转场
                // （golden 难度帧行区无主菜单字条实证）；灰态 label 3=淡出 5f。
                if (!interrupt(resources.main_root, 3, true)) return false;
                state.change_substate(4);
            } else if (selected == i32(TitleMainEntry::Quit)) {
                if (!sound(9)) return false;
                state.change_substate(4);
            } else {
                // 其余入口（真机有画面，本轮不实现）：仅确认音，不切屏、菜单保持可交互。
                if (!sound(7)) return false;
            }
        }
        break;
    }
    case 4:
        if (state.age.current > 19) enter(state.menu.cursor);
        break;
    }
    return true;
}

// g361 菜单展开：root0（spawn 3-22）+ 版本条 + 全项入场 + 禁用项黑化。
// 白态 13-22 子 VM 建后即毁——真机 boot 无白条绘制（oracle 全窗无 title01@L25
// ops；真机 ins_500 建后待中断的插值冻结语义 vs 移植侧自动淡入，引擎差异，
// 蓝图 §oracle 章登记，待 AnmUpdate 语义裁定后改为保留）。
bool TitleMainMenu::open_menu() {
    if (!present(state.handles[resources.main_root])) {
        if (!create(resources.main_root)) return false;
        auto* root = animations.registry.find(state.handles[resources.main_root]);
        if (!root) return fail("Title menu root unavailable");
        for (i32 white = 13; white <= 22; white++) {
            if (auto* child = animations.registry.find_child_script(*root, white, 0))
                animations.registry.discard(*child);
        }
    }
    if (!present(state.version_bar)) {
        state.version_bar = animations.create(resources.version, resources.version_bar,
                                              -1, 0);
        if (!state.version_bar) return fail(animations.error);
    }
    return show_menu();
}

// 选中项入场（菜单就绪时与移动共用）：interrupt(0,7+cursor)（th15 调用形状，
// 标签号兼容；paint()/30/31 废除——th13 为空操作）。
bool TitleMainMenu::select_entry() {
    return interrupt(resources.main_root, i32(i16(state.menu.cursor)) + 7, false);
}

}  // namespace th13
