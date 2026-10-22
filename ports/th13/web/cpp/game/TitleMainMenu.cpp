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

// [窗A 修正] 背景半张层值按 oracle 实测落码（蓝图 §8.3 层偏移现象；机制 =
// supervisor 逐 VM 置层推定，反汇编待锚）。oracle 证据（title-draws.jsonl）：
//   f349/g350 起 title00a 左半(entry1)@L23、右半(entry2)@L22（dump ins_304 为
//   22/21，各 +1）；select00 右半(entry4) boot 期 @L23（dump 21，+2），
//   g350 搬至 L21（与森林淡入同帧，substate 1 age==169 复刻）；select00 左半
//   (entry3)@L21 与 dump 一致不动。
void TitleMainMenu::adopt_background_layers() {
    auto* root = animations.registry.find(state.handles[resources.background_root]);
    if (!root) return;
    if (auto* forest_left = animations.registry.find_child_script(*root, 90, 0))
        forest_left->visual.layer = 23;
    if (auto* forest_right = animations.registry.find_child_script(*root, 91, 0))
        forest_right->visual.layer = 22;
    if (auto* overlay_right = animations.registry.find_child_script(*root, 93, 0))
        overlay_right->visual.layer = 23;
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
            // 仅新建时落层值（返回路径背景常驻，保留 g350 搬移后的状态）。
            adopt_background_layers();
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
        if (!returning && state.age.current == 169) {
            // [窗A 修正] oracle f349/g350：select00 右半自 L23 搬至 L21（与森林
            // title00a 淡入同帧；最后 23 帧 f348/g349、首个 21 帧 f349/g350，
            // 蓝图 §8.3 层搬移现象）。搬移机制（supervisor 逐 VM 置层）未反汇编。
            if (auto* root = animations.registry.find(state.handles[resources.background_root]))
                if (auto* overlay = animations.registry.find_child_script(*root, 93, 0))
                    overlay->visual.layer = 21;
        }
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
            if (selected == i32(TitleMainEntry::Quit)) {
                if (!sound(9)) return false;
                state.change_substate(4);
                break;
            }
            if (!sound(7)) return false;
            // [解剖轮2] Z 确认序重做（difficulty-menu-anatomy.md §③/⑥ + 顶点色）：
            // 主菜单 VM **不销毁**（解剖实证 3..12/13..22 转屏后仍在池、仅逐步
            // 隐出）——废除轮1 的 interrupt(0,6)/label2。帧级时间线（Z=age0）：
            //   +8  title_ver 末日（age==8 回收，次帧起无 op，oracle f477/478）；
            //   +10 灰条透明度线性衰减 -13/帧（oracle f479 起 0xff→0xf2→0xe5…，
            //       rgb 恒 192、禁用项黑 rgb 不变自 0xa0 起）——无对应标签序列，
            //       supervisor 置色推定，逐帧 poke（入场插值早已结束不覆写）；
            //   +14 LOGO 树回收（title_logo 末日 f483、f484 起无 op）；
            //   +20 enter() 转难度屏（难度内容 f490=Z+21 起在场）；
            //   +21 起的隐藏时间线（非选中灰条隐藏/选中白化续衰减/白条 9 枚闪
            //       8 帧）由难度屏接手（TitleDifficultyMenu，其 age0 = Z+21）。
            state.confirmed_entry = selected;
            state.change_substate(4);
            // 其余入口（真机有画面，本轮不实现）：确认序同上，enter() 不切屏
            // ——解剖实证字条隐藏与转屏解耦，难度侧时间线照走。
        }
        break;
    }
    case 4: {
        if (state.age.current == 8 && state.version_bar &&
            state.menu.cursor != kTitleMainEntries - 1) {
            // oracle g478（Z+8）title_ver 末日、g479 起无 op——立即回收对齐。
            animations.retire(state.version_bar);
        }
        if (state.age.current == 14 && state.handles[resources.logo_root]) {
            // oracle title_logo 末日 f483（Z+14）、f484 起无 op。
            animations.retire(state.handles[resources.logo_root]);
        }
        if (state.age.current >= 10 && state.age.current <= 30) fade_menu_alphas(state.age.current - 10);
        if (state.age.current > 19) enter(state.menu.cursor);
        break;
    }
    }
    return true;
}

// Z+10 起灰条透明度衰减（oracle f479-f489 顶点色线性 -13/帧；rgb 恒定，
// 禁用项黑 rgb 不变）。直接 poke visual.color。
void TitleMainMenu::fade_menu_alphas(i32 step) {
    auto* root = animations.registry.find(state.handles[resources.main_root]);
    if (!root) return;
    const i32 alpha = 255 - 13 * step, dimmed = 160 - 13 * step;
    for (i32 item = 0; item < kTitleMainEntries; item++) {
        auto* child = animations.registry.find_child_script(*root, item + 3, 0);
        if (!child) continue;
        const u32 rgb = child->visual.color & 0xffffffu;
        i32 a = item == i32(TitleMainEntry::ExtraStart) ? dimmed : alpha;
        if (a < 0) a = 0;
        if (a > 255) a = 255;
        child->visual.color = u32(a) << 24 | rgb;
    }
}

// g361 菜单展开：root0（spawn 3-22）+ 版本条 + 全项入场 + 禁用项黑化。
// 白态 13-22 子 VM 建后即毁——真机 boot 无白条绘制（oracle 全窗无 title01@L25
// ops；真机 ins_500 建后待中断的插值冻结语义 vs 移植侧自动淡入，引擎差异，
// 蓝图 §oracle 章登记，待 AnmUpdate 语义裁定后改为保留）。
// [解剖轮2] Z 序不再销毁字条 VM（解剖 §⑥：转屏后仍在池）——返回路径无需
// 整树重建，present 复用 + 全项重入场（label 7+k 恢复透明度/可见性）。
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
    // 选中项曾白化续衰减（Z 序）：恢复灰 192（label 28 = rgb→192 4f）。
    if (state.confirmed_entry >= 0 && state.confirmed_entry < kTitleMainEntries &&
        state.confirmed_entry != i32(TitleMainEntry::ExtraStart))
        child_interrupt(state.confirmed_entry + 3, 28, false);
    return show_menu();
}

// 选中项入场（菜单就绪时与移动共用）：interrupt(0,7+cursor)（th15 调用形状，
// 标签号兼容；paint()/30/31 废除——th13 为空操作）。
bool TitleMainMenu::select_entry() {
    return interrupt(resources.main_root, i32(i16(state.menu.cursor)) + 7, false);
}

}  // namespace th13
