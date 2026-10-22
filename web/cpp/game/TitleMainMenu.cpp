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
// [wf360 实测] boot 入场 = 全员 15 帧线性 alpha 渐升（oracle f360-373 逐帧
// 17/帧，rgb 恒 c0c0c0）+ 禁用项恒 0xa0000000；**无 label-7 脉动**（稳态
// f383-469 恒 ff 平直——旧 interrupt(7+k) 入场的 192↔255 色脉动不成立，
// oracle f410 恒 c0c0c0 vs port 旧值 dfdfdf 实证）。入场经逐帧 poke（见
// update substate 1 的 boot_fade）。
bool TitleMainMenu::show_menu() {
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
    ++menu_age;  // 单调屏龄（substate 切换会重置 state.age，渐入/闪场用此）
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
        boot_fade(menu_age);
        if (state.age.current >= menu_at + kTitleMenuReadyDelay) {
            // g372 起可交互（oracle 实测）；闪场于就绪当帧武装（f371 首帧
            // 0x8e=142，此后降段 4 帧/隐 4 帧/升 3 帧至 ff）。
            if (!select_entry()) return false;
            state.change_substate(2);
        }
        break;
    }
    case 2: {
        state.menu.previous = state.menu.cursor;
        if ((pressed | repeated) & 0x10) state.menu.move(-1);  // ↑=0x10（oracle 位布局）
        if ((pressed | repeated) & 0x20) state.menu.move(1);   // ↓=0x20
        if (!state.menu.error.empty()) return fail(state.menu.error);
        if (state.menu.previous != state.menu.cursor) {
            // 仅新项入场（oracle f419-423 移动形：4 帧降至 {163,91,40,10} 后
            // 回 ff 平直；无脉动、无隐藏段——与就绪闪场不同形）。
            if (!sound(10)) return false;
            if (entry_flash >= 0) set_item_alpha(entry_flash, 255);  // 旧闪场复位
            entry_flash = state.menu.cursor;
            entry_flash_step = 0;
            move_flash = true;
        }
        boot_fade(menu_age);  // 渐入尾段跨过就绪帧（f371-373 仍升）
        if (entry_flash >= 0) advance_entry_flash();
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

// 逐项 alpha poke（灰条入场插值早已结束/未开始，poke 不被覆写）。
// rgb 恒 (192,192,192)；禁用项恒黑 rgb（oracle 全程 0xa0000000）。
void TitleMainMenu::set_item_alpha(i32 item, i32 alpha) {
    auto* root = animations.registry.find(state.handles[resources.main_root]);
    if (!root) return;
    auto* child = animations.registry.find_child_script(*root, item + 3, 0);
    if (!child) return;
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    const u32 rgb = item == i32(TitleMainEntry::ExtraStart) ? 0u : 0xc0c0c0u;
    child->visual.color = u32(alpha) << 24 | rgb;
}

// boot 15 帧线性淡入：oracle f360(alpha 0x22/0x33=帧 2/3)→f373 ff，17/帧。
// 菜单展开帧 = 帧 0（age==menu_at），次帧可见。
void TitleMainMenu::boot_fade(i32 frame) {
    if (frame < 0 || frame > 15) return;
    // 线性段帧 0-11（17/帧至 221=0xdd）；实测尾段帧 12→249(0xf9)、13+→255
    // （oracle f370 dd → f371 f9 → f373 ff，尾段非 17/帧）。
    i32 alpha = 17 * (frame + 2);
    if (alpha > 204) alpha = frame >= 12 ? 255 : 249;
    for (i32 item = 0; item < kTitleMainEntries; item++) {
        if (item == i32(TitleMainEntry::ExtraStart)) continue;
        if (item == entry_flash) continue;  // 闪场序列优先（f371 起分道）
        set_item_alpha(item, alpha);
    }
    dim_disabled();
}

// 选中项闪场（select_entry 触发）：实测序列（f371-381）
// 降段 4 帧 {199,142,84,27}（≈-57/帧）、隐 4 帧、升段 3 帧 {214,244,255}。
// 帧 0 与调用同帧生效（oracle f371 当帧已 0x8e）。
void TitleMainMenu::advance_entry_flash() {
    // 移动形（f419-423）：{163,91,40,10} 后回 ff（5 步收束）。
    static const i32 kMove[4] = {163, 91, 40, 10};
    // 就绪形（f371-381）：降 {142,79,35,8}、隐 4 帧、升 {214,244,255}。
    static const i32 kDown[4] = {142, 79, 35, 8};
    static const i32 kUp[3] = {214, 244, 255};
    const i32 step = entry_flash_step++;
    i32 alpha;
    if (move_flash) {
        if (step < 4) alpha = kMove[step];
        else {
            set_item_alpha(entry_flash, 255);
            entry_flash = -1;
            move_flash = false;
            return;
        }
    } else if (step < 4) alpha = kDown[step];
    else if (step < 8) alpha = -1;  // 隐藏（alpha 0——批内消失）
    else if (step < 11) alpha = kUp[step - 8];
    else {
        set_item_alpha(entry_flash, 255);
        entry_flash = -1;
        return;
    }
    set_item_alpha(entry_flash, alpha < 0 ? 0 : alpha);
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
    menu_age = 0;
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

// [wf371 实测] 菜单就绪（f371=age191）选中项闪场：alpha 255→0 约 4 帧线性
// （f371-374: 0x8e/0x4f/0x23/0x08）、隐 4 帧（f375-378 批内消失）、
// 3 帧回升至 ff（f379-381: 0xd6/0xf4/0xff）后稳态平直。移动光标的入场
// 同为短淡入至 ff 平直（无脉动）。经 alpha poke 序列驱动（update 内）。
bool TitleMainMenu::select_entry() {
    entry_flash = state.menu.cursor;
    entry_flash_step = 0;
    move_flash = false;
    advance_entry_flash();  // 当帧生效（oracle f371 首帧即 0x8e）
    return true;
}

}  // namespace th13
