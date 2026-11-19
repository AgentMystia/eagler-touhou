#include "TitleScene.hpp"
#include "GameMenu.hpp"
#include "ReplayFile.hpp"

#include <cstdio>

namespace th16 {

namespace {
constexpr i32 kDecor61 = 0x61;
constexpr i32 kDecor65 = 0x65;
constexpr i32 kDifficultyBack = 0x68;
constexpr i32 kShotBack = 0x69;
constexpr i32 kSeasonBack = 0x6a;
constexpr i32 kStageBack = 0x71;
constexpr i32 kShotPortrait = 0x5c;  // +0x430：机体画面左右切换时淡出的 VM
constexpr i32 kExtraDifficulty = 4;

i32 to_short(i32 v) { return i32(i16(v)); }
}  // namespace

u32 TitleScene::child_id(AnmManager& anm, i32 slot, i32 script) {
    AnmVm* parent = anm.find(vm[slot]);
    if (parent == nullptr) { vm[slot] = 0; return 0; }
    AnmVm* child = AnmManager::find_child(*parent, script, 0);
    return child ? child->id : 0;
}

void TitleScene::item_interrupt_now(AnmManager& anm, i32 script, i32 value) {
    const u32 id = child_id(anm, kLogo, script);
    AnmVm* item = anm.find(id);
    if (item == nullptr) return;
    item->pending_interrupt = value;
    anm.run(*item);
}

void TitleScene::item_interrupt(AnmManager& anm, i32 script, i32 value) {
    AnmVm* item = anm.find(child_id(anm, kLogo, script));
    if (item) item->pending_interrupt = value;
}

void TitleScene::dismiss(AnmManager& anm, i32 slot) {
    anm.interrupt(vm[slot], 1);
    vm[slot] = 0;
}

void TitleScene::reveal(AnmManager& anm, u32 id) {
    AnmVm* root = anm.find(id);
    if (root == nullptr) return;
    struct Rec {
        static void go(AnmVm& v) {
            v.flags_lo &= ~0x2u;
            for (AnmListNode* n = v.children.next; n != nullptr; n = n->next) if (n->owner) go(*n->owner);
        }
    };
    Rec::go(*root);
}

void TitleScene::spawn_into(AnmManager& anm, i32 slot, AnmFile& file, i32 script) {
    vm[slot] = anm.spawn(file, script, -1);
}

void TitleScene::request_game(i32 stage, i32 table) {
    globals->stage_table = table;
    globals->stage = stage;
    globals->stage_copy = stage;
    globals->scene_request = kSceneGame;
    events.push_back({"scene", kSceneGame});
}

void TitleScene::transition_effect() {
    // 0x41a390：ascii.anm 0x11（Loading 文字）；0x418ba0：EffectManager 种类 0 过场；0x46f0b0(..., 7) 中断 7
    events.push_back({"transition", 7});
    if (on_transition_effect) on_transition_effect();
}

void TitleScene::tick_idle_demo(const InputState& input) {
    // 0x44af80 开头：画面==1 时 DAT_004a5bf0++；held==0 且 ≥0x708 → bit6 demo + 场景 0xd
    if (screen != kMainMenu || globals == nullptr) return;
    ++idle_frames;
    if (input.held == 0) {
        if (idle_frames < 0x708) return;
        GameGlobals& g = *globals;
        g.flags = (g.flags & ~0x80u) | 0x40u;
        pending_demo = demo_index;
        events.push_back({"demo", demo_index});
        const i32 n = demo_index + 1;
        const i64 prod = i64(n) * 0x55555555ll;
        i32 edx = i32(u32(u64(prod) >> 32));
        edx = edx - n;
        edx >>= 1;
        i32 ecx = edx;
        ecx = i32(u32(ecx) >> 31);
        ecx = ecx + edx;
        demo_index = ecx * 3 + n;
        g.scene_request = 0xd;
        events.push_back({"scene", g.scene_request});
        idle_frames = 0;
    } else {
        idle_frames = 0;
    }
}

// 0x44af80
void TitleScene::update(AnmManager& anm, const InputState& input) {
    if (title_file == nullptr) return;
    tick_idle_demo(input);
    switch (screen) {
    case kNone: {
        // 0x44af80 case 0：按 DAT_004a6f1c（title_origin）分流
        const i32 origin = globals->title_origin;
        if (origin == 0) {
            globals->title_origin = 1;
            set_screen(kMainMenu);
            update_main_menu(anm, input);
        } else if (origin == 1) {
            set_screen(kMainMenu);
            update_main_menu(anm, input);
        } else if (origin == 2) {
            // 回放结束回标题：进 Replay 画面（BGM th16_01 等由宿主补）
            globals->title_origin = 1;
            set_screen(kReplay);
        } else if (origin == 5) {
            // 符卡练习返回：kind 9 淡入 + 装饰 0x61 interrupt 3 → Spell Practice
            if (on_screen_fade) on_screen_fade(9, 0x1e, 0);
            globals->title_origin = 1;
            u32 decor = 0;
            if (vm[kDecor61] == 0) spawn_into(anm, kDecor61, *title_file, kDecor61);
            decor = vm[kDecor61];
            if (decor) anm.interrupt_now(decor, 3);
            set_screen(kSpellPractice);
        } else if (origin == 4) {
            // 练习返回：kind 9 淡入 + 装饰 0x61 interrupt 3 → 难度选择
            if (on_screen_fade) on_screen_fade(9, 0x1e, 0);
            u32 decor = 0;
            if (vm[kDecor61] == 0) spawn_into(anm, kDecor61, *title_file, kDecor61);
            decor = vm[kDecor61];
            if (decor) anm.interrupt_now(decor, 3);
            set_screen(kDifficulty);
        } else if (origin == 3) {
            globals->title_origin = 1;
            set_screen(kMusicRoom);
        } else {
            globals->title_origin = 1;
            set_screen(kMainMenu);
            update_main_menu(anm, input);
        }
        break;
    }
    case kMainMenu:
        update_main_menu(anm, input);
        break;
    case kFinished: {
        // 场景请求：游戏开始路径在上一帧已置 7（Supervisor 先于本链消费）；否则为退出 2/3
        if (globals->scene_request < 0) {
            globals->scene_request = 2;
            events.push_back({"scene", 2});
        }
        break;
    }
    case kDifficulty: update_difficulty(anm, input); break;
    case kShot: update_shot(anm, input); break;
    case kSeason: update_season(anm, input); break;
    case kStage: update_stage(anm, input); break;
    case kOption: update_option(anm, input); break;
    case kReplay: update_replay(anm, input); break;
    case kManual: update_manual(anm, input); break;
    case kMusicRoom: update_music(anm, input); break;
    case kSpellPractice: update_spell_practice(anm, input); break;
    case kSpellList: update_spell_list(anm, input); break;
    case kSpellShot: update_spell_shot(anm, input); break;
    case kScore: update_score(anm, input); break;
    case kKeyConfig: update_key_config(anm, input); break;
    default: update_stub(anm, input); break;
    }
    ++timer;  // 0x44af80 末尾
}

// 主菜单子项：光标前的项（3+i / 13+i）→ 0x1e，光标后 → 0x1f；无通关数据时项 1（4 / 0xe）→ 0x1d
void TitleScene::refresh_main_items(AnmManager& anm) {
    i32 i = 0;
    for (; i < cursor.index; ++i) {
        item_interrupt_now(anm, i + 3, 0x1e);
        item_interrupt_now(anm, i + 0xd, 0x1e);
    }
    if (i + 1 < 10) {
        for (i += 0xe; i < 0x17; ++i) {
            item_interrupt_now(anm, i - 10, 0x1f);
            item_interrupt_now(anm, i, 0x1f);
        }
    }
    if (!has_clear_data) {
        item_interrupt(anm, 4, 0x1d);
        item_interrupt(anm, 0xe, 0x1d);
    }
}

// 0x44b5f0
void TitleScene::update_main_menu(AnmManager& anm, const InputState& input) {
    GameGlobals& g = *globals;
    switch (sub) {
    case 0: {
        cursor.count = 10;
        cursor.wrap = true;
        if (!has_clear_data) cursor.disable(1);
        const i32 mode = g.mode();
        if (mode == 2) { cursor.set(3); g.set_mode(0); }
        else if (mode != 0) { cursor.set(2); g.set_mode(0); }
        set_sub(1);
        if (anm.find(vm[0x61]) == nullptr) {
            spawn_into(anm, 0x61, *title_file, 0x61);
            anm.interrupt_now(vm[0x61], 2);
        }
        if (anm.find(vm[kDecor65]) == nullptr) {
            spawn_into(anm, kDecor65, *title_file, kDecor65);
            anm.interrupt_now(vm[kDecor65], 2);
        }
        if (prev_screen != kOption) anm.interrupt_now(vm[0x61], 2);
        timer = 0x78;  // 0x406490(&Timer, 0x78)
        [[fallthrough]];
    }
    case 1:
        if (timer == 0x78) {
            spawn_into(anm, kLogo, *title_file, 0);
            if (anm.find(vm[kTitleV]) == nullptr) {
                vm[kTitleV] = 0;
                spawn_into(anm, kTitleV, *title_v_file, 0);
            }
            refresh_main_items(anm);
        }
        if (timer > 0x82) {
            sub = 2;
            timer = 0;
            anm.interrupt_now(vm[kLogo], 3);
            anm.interrupt(vm[kLogo], to_short(cursor.index + 0x11));
            refresh_main_items(anm);
        }
        break;
    case 2: {
        prev_cursor = cursor.index;
        const u32 edge = input.pressed | input.repeat;
        if (edge & InputState::kUp) cursor.move(-1);
        if (edge & InputState::kDown) cursor.move(1);
        if (prev_cursor != cursor.index) {
            sfx(10);
            anm.interrupt_now(vm[kLogo], 3);
            anm.interrupt(vm[kLogo], to_short(cursor.index + 7));
            refresh_main_items(anm);
        }
        bool confirm = (input.pressed & InputState::kConfirmMask) != 0;
        if (input.pressed & InputState::kCancelMask) {
            if (cursor.index == 9) {
                confirm = true;  // switchD_0044c258_caseD_9
            } else {
                sfx(9);
                cursor.set(9);
                anm.interrupt_now(vm[kLogo], 3);
                anm.interrupt(vm[kLogo], to_short(cursor.index + 7));
                refresh_main_items(anm);
            }
        }
        if (confirm) {
            anm.interrupt(vm[kLogo], 6);
            switch (cursor.index) {
            case 7:
                sfx(7);
                break;
            case 9:
                sfx(9);
                break;
            default:
                sfx(7);
                anm.interrupt(vm[kDecor65], 1);
                vm[kDecor65] = 0;
                anm.interrupt(vm[kTitleV], 1);
                break;
            }
            set_sub(4);
        }
        break;
    }
    case 4:
        if (timer > 0x13) {
            switch (cursor.index) {
            case 0:
                g.set_mode(0);
                anm.interrupt_now(vm[0x61], 3);
                set_screen(kDifficulty);
                cursor.push();
                cursor.set(g.last_difficulty);
                g.difficulty = g.last_difficulty;
                break;
            case 1:
                g.set_mode(0);
                anm.interrupt_now(vm[0x61], 3);
                set_screen(kDifficulty);
                cursor.push();
                g.difficulty = kExtraDifficulty;
                cursor.set(0);
                break;
            case 2:
                g.set_mode(1);
                anm.interrupt_now(vm[0x61], 3);
                cursor.push();
                cursor.set(g.last_difficulty);
                g.difficulty = g.last_difficulty;
                set_screen(kDifficulty);
                break;
            case 3:
                g.set_mode(2);
                anm.interrupt_now(vm[0x61], 3);
                set_screen(kSpellPractice);
                cursor.push();
                cursor.set(0);
                break;
            case 4: anm.interrupt_now(vm[0x61], 3); set_screen(kReplay); cursor.push(); break;
            case 5: anm.interrupt_now(vm[0x61], 3); set_screen(kScore); cursor.push(); break;
            case 6: anm.interrupt_now(vm[0x61], 3); set_screen(kMusicRoom); cursor.push(); break;
            case 7: set_screen(kOption); cursor.push(); break;
            case 8: anm.interrupt_now(vm[0x61], 3); set_screen(kManual); cursor.push(); break;
            case 9: set_screen(kFinished); break;
            default: break;
            }
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

// 0x44fe20：难度选择（面板脚本 0x7e 普通 / 0x7f Extra；项脚本 0x74+d；通关标记 0x9e+d / 0xa2）
void TitleScene::update_difficulty(AnmManager& anm, const InputState& input) {
    GameGlobals& g = *globals;
    const bool extra = g.difficulty > 3;
    const i32 panel = (extra ? 1 : 0) + 0x7e;
    switch (sub) {
    case 0: {
        if (vm[kLeafEmitter] == 0 && ascii_file) spawn_into(anm, kLeafEmitter, *ascii_file, kLeafEmitterScript);
        cursor.wrap = false;
        cursor.count = extra ? 1 : 4;
        anm.interrupt(vm[panel], 1);
        vm[panel] = 0;
        spawn_into(anm, panel, *title_file, panel);
        anm.interrupt_now(vm[panel], 3);
        anm.interrupt(vm[panel], to_short(cursor.index + 0xd));
        // DAT_004a6f1c == 4（练习返回直达机体选择）待接
        spawn_into(anm, kDifficultyBack, *title_file, kDifficultyBack);
        set_sub(1);
        // 无通关记录的难度显示提示标记（0x44a8e0 == 0 → reveal 子 0x9e+d）；当前无存档 → 全部显示
        if (!extra) {
            for (i32 d = 0; d < 4; ++d) reveal(anm, child_id(anm, panel, 0x9e + d));
        } else {
            reveal(anm, child_id(anm, panel, 0xa2));
        }
        [[fallthrough]];
    }
    case 1:
        if (timer > 6) set_sub(2);
        break;
    case 2: {
        if (!extra) {
            prev_cursor = cursor.index;
            const u32 edge = input.pressed | input.repeat;
            if (edge & (InputState::kUp | InputState::kLeft)) cursor.move(-1);
            if (edge & (InputState::kDown | InputState::kRight)) cursor.move(1);
            if (prev_cursor != cursor.index) {
                sfx(10);
                anm.interrupt_now(vm[panel], 3);
                anm.interrupt(vm[panel], to_short(cursor.index + 7));
            }
        }
        if (input.pressed & InputState::kCancelMask) {
            set_sub(4);
            sfx(9);
            dismiss(anm, panel);
            return;
        }
        if (input.pressed & InputState::kConfirmMask) {
            anm.interrupt(vm[panel], 6);
            anm.interrupt(child_id(anm, panel, extra ? 0x78 : cursor.index + 0x74), 2);
            set_sub(3);
            sfx(7);
        }
        break;
    }
    case 3:
        if (timer > 0xd) {
            dismiss(anm, kDifficultyBack);
            // LAB_004501a0
            set_screen(kShot);
            if (!extra) {
                g.last_difficulty = cursor.index;
                g.difficulty = cursor.index;
            }
            cursor.push();
            cursor.wrap = true;
            cursor.count = 4;
            cursor.set(g.last_shot);
            g.shot = g.last_shot;
            events.push_back({"screen", screen});
        }
        break;
    case 4:
        if (timer > 5) {
            dismiss(anm, kDifficultyBack);
            if ((g.flags & GameGlobals::kModeMask) == 0) {
                if (!extra) {
                    set_screen(kMainMenu);
                    g.difficulty = cursor.index;
                    g.last_difficulty = cursor.index;
                    cursor.pop();
                    events.push_back({"screen", screen});
                    return;
                }
            } else {
                g.last_difficulty = cursor.index;
            }
            g.difficulty = g.last_difficulty;
            set_screen(kMainMenu);
            dismiss(anm, kLeafEmitter);
            set_screen(kMainMenu);
            cursor.pop();
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

// 0x4502c0：机体选择（面板 0x96 普通 / 0x98 Extra；项 0x80+s / 0x88+s；通关标记 0x9a..0x9d；装饰 0x58/0x59）
void TitleScene::update_shot(AnmManager& anm, const InputState& input) {
    GameGlobals& g = *globals;
    const bool extra = g.difficulty == kExtraDifficulty;
    const i32 panel = (extra ? 2 : 0) + 0x96;
    switch (sub) {
    case 0: {
        cursor.count = 4;
        if (extra) {
            // 0x44a800：该机体是否解锁 Extra；无存档 → 全部禁用
            for (i32 s = 0; s < 4; ++s) cursor.disable(s);
        }
        if (anm.find(vm[kShotBack]) == nullptr) spawn_into(anm, kShotBack, *title_file, kShotBack);
        if (anm.find(vm[panel]) == nullptr) {
            anm.interrupt(vm[panel], 1);
            vm[panel] = 0;
            spawn_into(anm, panel, *title_file, panel);
        }
        anm.interrupt_now(vm[panel], 3);
        anm.interrupt(vm[panel], to_short(cursor.index + 7));
        set_sub(1);
        // 该难度下各机体无通关记录 → reveal 0x9a..0x9d（当前无存档 → 全部）
        for (i32 k = 0; k < 4; ++k) reveal(anm, child_id(anm, panel, 0x9a + k));
        // DAT_004a6f1c == 4 分支待接
        [[fallthrough]];
    }
    case 1:
        if (timer > 6) set_sub(2);
        break;
    case 2: {
        prev_cursor = cursor.index;
        const u32 edge = input.pressed | input.repeat;
        if (edge & InputState::kLeft) {
            dismiss(anm, kShotPortrait);
            sfx(10);
            anm.interrupt_now(vm[panel], to_short(cursor.index + 0x19));
            cursor.move(-1);
            anm.interrupt(vm[panel], to_short(cursor.index + 0xd));
        }
        if (edge & InputState::kRight) {
            dismiss(anm, kShotPortrait);
            sfx(10);
            anm.interrupt_now(vm[panel], to_short(cursor.index + 0x13));
            cursor.move(1);
            anm.interrupt(vm[panel], to_short(cursor.index + 7));
        }
        if (input.pressed & InputState::kCancelMask) {
            set_sub(4);
            sfx(9);
            return;
        }
        if (input.pressed & InputState::kConfirmMask) {
            anm.interrupt(child_id(anm, panel, cursor.index + 0x80), 6);
            anm.interrupt(child_id(anm, panel, cursor.index + 0x88), 6);
            anm.interrupt(child_id(anm, panel, 0x58), 6);
            anm.interrupt(child_id(anm, panel, 0x59), 6);
            sfx(7);
            set_sub(3);
        }
        break;
    }
    case 3:
        if (timer < 0xe) return;
        // LAB_004509e1
        set_screen(kSeason);
        g.shot = cursor.index;
        g.last_shot = g.shot;
        cursor.push();
        cursor.disabled_count = 0;
        cursor.wrap = true;
        cursor.count = 5;
        cursor.set(g.shot);
        anm.interrupt(vm[panel], 6);
        dismiss(anm, kShotBack);
        events.push_back({"screen", screen});
        break;
    case 4:
        if (timer > 5) {
            dismiss(anm, panel);
            dismiss(anm, kShotBack);
            set_screen(kDifficulty);
            g.shot = cursor.index;
            cursor.pop();
            g.last_shot = g.shot;
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

// 0x450af0：子季节选择（面板 0x97 普通 / 0x99 Extra）
void TitleScene::update_season(AnmManager& anm, const InputState& input) {
    GameGlobals& g = *globals;
    const bool extra = g.difficulty == kExtraDifficulty;
    const i32 panel = (extra ? 2 : 0) + 0x97;
    switch (sub) {
    case 0:
        cursor.count = 4;
        if (extra) { cursor.set(0); cursor.count = 1; }
        anm.interrupt(vm[panel], 1);
        spawn_into(anm, panel, *title_file, panel);
        anm.interrupt_now(vm[panel], 3);
        anm.interrupt_now(vm[panel], to_short(cursor.index + 7));
        anm.interrupt_now(vm[panel], to_short(g.shot + 0x1f));
        set_sub(1);
        spawn_into(anm, kSeasonBack, *title_file, kSeasonBack);
        // DAT_004a6f1c == 4 分支待接
        [[fallthrough]];
    case 1:
        if (timer > 6) set_sub(2);
        break;
    case 2: {
        prev_cursor = cursor.index;
        if (!extra) {
            const u32 edge = input.pressed | input.repeat;
            if (edge & InputState::kLeft) {
                sfx(10);
                anm.interrupt_now(vm[panel], to_short(cursor.index + 0x19));
                cursor.move(-1);
                anm.interrupt(vm[panel], to_short(cursor.index + 0xd));
            }
            if (edge & InputState::kRight) {
                sfx(10);
                anm.interrupt_now(vm[panel], to_short(cursor.index + 0x13));
                cursor.move(1);
                anm.interrupt(vm[panel], to_short(cursor.index + 7));
            }
        }
        if (input.pressed & InputState::kCancelMask) {
            set_sub(4);
            sfx(9);
            return;
        }
        if (input.pressed & InputState::kConfirmMask) {
            sfx(7);
            set_sub(3);
            sfx(0x32);
            if ((g.flags & GameGlobals::kModeMask) == 0) events.push_back({"bgm-fade", 0});  // 0x43c470
        }
        break;
    }
    case 3:
        if (timer == 10) {
            if ((g.flags & GameGlobals::kModeMask) != 0) {
                // LAB_00450d8f：练习 / 符卡练习 → 关卡选择
                g.season = cursor.index;
                cursor.push();
                dismiss(anm, panel);
                dismiss(anm, kSeasonBack);
                set_screen(kStage);
                events.push_back({"screen", screen});
                return;
            }
            transition_effect();
        }
        if (timer > 0x27) {
            g.season = extra ? 4 : cursor.index;
            cursor.push();
            g.replay_slot = -1;
            set_screen(kFinished);
            if (extra) request_game(7, 1);
            else request_game(1, 0);
        }
        break;
    case 4:
        if (timer > 5) {
            g.season = cursor.index;
            dismiss(anm, panel);
            dismiss(anm, kSeasonBack);
            set_screen(kShot);
            cursor.pop();
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

// 0x450ef0：练习关卡选择（底 0x71；6 关）
void TitleScene::update_stage(AnmManager& anm, const InputState& input) {
    GameGlobals& g = *globals;
    switch (sub) {
    case 0:
        cursor.count = 6;
        cursor.set(g.last_practice_stage);
        spawn_into(anm, kStageBack, *title_file, kStageBack);
        set_sub(1);
        if (g.title_origin == 4) g.title_origin = 1;
        [[fallthrough]];
    case 1:
        if (timer > 10) set_sub(2);
        break;
    case 2: {
        prev_cursor = cursor.index;
        const u32 edge = input.pressed | input.repeat;
        if (edge & InputState::kUp) cursor.move(-1);
        if (edge & InputState::kDown) cursor.move(1);
        if (prev_cursor != cursor.index) sfx(10);
        if (input.pressed & InputState::kCancelMask) {
            set_sub(4);
            sfx(9);
            g.last_practice_stage = cursor.index;
            return;
        }
        if (input.pressed & InputState::kConfirmMask) {
            // 存档中 (机体+季节, 难度, 关卡) 的练习解锁位；无存档 → 全部未解锁
            const bool unlocked = false;
            if (!unlocked) { sfx(0x10); return; }
            set_sub(3);
            sfx(7);
            sfx(0x32);
            g.last_practice_stage = cursor.index;
            events.push_back({"bgm-fade", 0});
        }
        break;
    }
    case 3:
        if (timer == 10) transition_effect();
        if (timer > 0x27) {
            cursor.push();
            set_screen(kFinished);
            g.title_origin = 4;
            g.last_practice_stage = cursor.index;
            request_game(cursor.index + 1, 2);
        }
        break;
    case 4:
        if (timer > 5) {
            dismiss(anm, kStageBack);
            set_screen(kSeason);
            cursor.pop();
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

void TitleScene::panel_item_interrupt_now(AnmManager& anm, i32 script, i32 value) {
    const u32 id = child_id(anm, 1, script);  // Option 面板 +0x2c4 = vm[1]
    AnmVm* item = anm.find(id);
    if (item == nullptr) return;
    item->pending_interrupt = value;
    anm.run(*item);
}

void TitleScene::refresh_option_items(AnmManager& anm) {
    // 0x44c8c0：光标前项脚本 i+0x17 / i+0x1c → 0x1e；光标后项 → 0x1f（选中由面板 interrupt 负责）
    if (vm[1] == 0) return;
    for (i32 i = 0; i < cursor.index; ++i) {
        panel_item_interrupt_now(anm, i + 0x17, 0x1e);
        panel_item_interrupt_now(anm, i + 0x1c, 0x1e);
    }
    for (i32 i = cursor.index + 1; i < 5; ++i) {
        panel_item_interrupt_now(anm, i + 0x17, 0x1f);
        panel_item_interrupt_now(anm, i + 0x1c, 0x1f);
    }
}

void TitleScene::apply_option_volume(AnmManager& anm) {
    // 0x44dc70：把 BGM/SE 写到面板子脚本 0x21.. 的数字 ANM；无子 VM 时静默跳过
    (void)anm;
    if (vm[1] == 0) return;
    // 宿主音量侧效在 Sound 子系统接入后再写 DAT_004df49c 族；此处保持逻辑值即可
}

// 0x44c570：Option（BGM / SE / KeyConfig / Default / Quit）
void TitleScene::update_option(AnmManager& anm, const InputState& input) {
    GameGlobals& g = *globals;
    switch (sub) {
    case 0:
        cursor.count = 5;
        cursor.wrap = true;
        cursor.set(0);
        if (title_file != nullptr) spawn_into(anm, 1, *title_file, 1);  // +0x2c4 面板脚本 1
        apply_option_volume(anm);
        refresh_option_items(anm);
        set_sub(1);
        events.push_back({"option", 0});
        [[fallthrough]];
    case 1:
        if (timer > 6) {
            set_sub(2);
            if (vm[1]) {
                anm.interrupt_now(vm[1], 3);
                anm.interrupt(vm[1], cursor.index + 0x11);
            }
            refresh_option_items(anm);
        }
        break;
    case 2: {
        const i32 prev = cursor.index;
        const u32 edge = input.pressed | input.repeat;
        if (edge & InputState::kUp) cursor.move(-1);
        if (edge & InputState::kDown) cursor.move(1);
        if (cursor.index != prev) {
            sfx(10);
            if (vm[1]) {
                anm.interrupt_now(vm[1], 3);
                anm.interrupt(vm[1], to_short(cursor.index + 7));
            }
            refresh_option_items(anm);
        }
        // 取消：非 Quit 项时先跳到 Quit（4），再取消才退出——与原版 0x102 分支一致
        if (input.pressed & InputState::kCancelMask) {
            if (cursor.index != 4) {
                sfx(9);
                cursor.set(4);
                if (vm[1]) {
                    anm.interrupt_now(vm[1], 3);
                    anm.interrupt(vm[1], to_short(cursor.index + 7));
                }
                refresh_option_items(anm);
                break;
            }
            if (vm[1]) anm.interrupt(vm[1], 6);
            sfx(9);
            set_sub(4);
            break;
        }
        // 左右调音量（项 0 BGM / 1 SE），步进 5，夹到 0..100（0x4186f0）
        if (edge & InputState::kLeft) {
            if (cursor.index == 0) {
                g.bgm_volume = u8(g.bgm_volume < 5 ? 0 : g.bgm_volume - 5);
                apply_option_volume(anm);
            } else if (cursor.index == 1) {
                g.se_volume = u8(g.se_volume < 5 ? 0 : g.se_volume - 5);
                apply_option_volume(anm);
            }
        }
        if (edge & InputState::kRight) {
            if (cursor.index == 0) {
                g.bgm_volume = u8(g.bgm_volume + 5 > 100 ? 100 : g.bgm_volume + 5);
                apply_option_volume(anm);
            } else if (cursor.index == 1) {
                g.se_volume = u8(g.se_volume + 5 > 100 ? 100 : g.se_volume + 5);
                apply_option_volume(anm);
            }
        }
        if (input.pressed & InputState::kConfirmMask) {
            if (cursor.index == 2) {
                if (vm[1]) anm.interrupt(vm[1], 6);
                sfx(7);
                set_sub(4);
            } else if (cursor.index == 3) {
                g.bgm_volume = 100;
                g.se_volume = 80;  // 0x5064
                g.option_flags = 0;
                apply_option_volume(anm);
                sfx(7);
            } else if (cursor.index == 4) {
                if (vm[1]) anm.interrupt(vm[1], 6);
                sfx(9);
                set_sub(4);
            }
        }
        break;
    }
    case 4:
        if (timer > 9) {
            if (cursor.index == 2) {
                dismiss(anm, 1);
                set_screen(kKeyConfig);
                cursor.push();
                events.push_back({"screen", screen});
            } else if (cursor.index == 4) {
                dismiss(anm, 1);
                set_screen(kMainMenu);
                cursor.pop();
                events.push_back({"screen", screen});
            }
        }
        break;
    default:
        break;
    }
}

void TitleScene::scan_replay_slots() {
    // 同步完成原版 0x402fb0 工作线程：标记 th16_01..th16_100.rpy
    for (int i = 0; i < 100; ++i) {
        replay_slot_live[i] = false;
        replay_stage_mask[i] = 0;
        replay_spell_practice[i] = false;
        if (replay_directory.empty()) continue;
        char path[512];
        std::snprintf(path, sizeof(path), "%s/th16_%02d.rpy", replay_directory.c_str(), i + 1);
        ReplayFile file;
        if (!file.open_file(path)) {
            // 兼容扁平 replay/ 子目录
            std::snprintf(path, sizeof(path), "%s/replay/th16_%02d.rpy", replay_directory.c_str(), i + 1);
            if (!file.open_file(path)) continue;
        }
        replay_slot_live[i] = true;
        if (file.user.size() >= 0xb) replay_spell_practice[i] = (file.user[0xa] & 2) != 0;
        for (const ReplayStageSegment& seg : file.stages) {
            if (seg.stage >= 1 && seg.stage <= 7) replay_stage_mask[i] |= 1 << (seg.stage - 1);
        }
    }
    replay_ui_flags = (replay_ui_flags & 0xfffffff3u) | 8u;  // 清 bit2、置 bit3 扫描完成
}

// 0x451750：标题 Replay（25 槽 × 3 页；确认 → 关卡选择 → 场景 0xd）
void TitleScene::update_replay(AnmManager& anm, const InputState& input) {
    switch (sub) {
    case 0: {
        cursor.count = 0x19;
        cursor.wrap = true;
        cursor.set(last_replay_slot % 0x19);
        page_cursor.count = 3;
        page_cursor.wrap = true;
        page_cursor.set(last_replay_slot / 0x19);
        // *(+0x2a4)=1：原版列表滚动相关；端口不需要独立字段
        last_replay_slot = 0;
        if (vm[kLeafEmitter] == 0 && ascii_file) spawn_into(anm, kLeafEmitter, *ascii_file, kLeafEmitterScript);
        if (title_file) spawn_into(anm, 0x6c, *title_file, 0x6c);
        set_sub(1);
        for (bool& live : replay_slot_live) live = false;
        replay_ui_flags &= 0xfffffff3u;
        replay_choice = -1;
        scan_replay_slots();
        if (vm[0x61] == 0 && title_file) {
            spawn_into(anm, 0x61, *title_file, 0x61);
            anm.interrupt_now(vm[0x61], 3);
        }
        events.push_back({"replay", 0});
        [[fallthrough]];
    }
    case 1:
        if (timer > 6) set_sub(2);
        break;
    case 2: {
        const i32 prev_slot = cursor.index;
        const i32 prev_page = page_cursor.index;
        const u32 edge = input.pressed | input.repeat;
        if (edge & InputState::kUp) cursor.move(-1);
        if (edge & InputState::kDown) cursor.move(1);
        if (edge & InputState::kLeft) page_cursor.move(-1);
        if (edge & InputState::kRight) page_cursor.move(1);
        if (page_cursor.index != prev_page) sfx(10);
        if (cursor.index != prev_slot) sfx(10);
        if (input.pressed & InputState::kCancelMask) {
            set_sub(5);
            sfx(9);
            replay_ui_flags |= 4u;
            break;
        }
        if (input.pressed & InputState::kConfirmMask) {
            const i32 abs = replay_abs_slot();
            if (abs < 0 || abs >= 100 || !replay_slot_live[abs]) break;
            set_sub(4);
            replay_choice = abs;
            cursor.push();
            sfx(7);
            cursor.count = 7;
            cursor.set(0);
            for (i32 i = 0; i < 7; ++i) {
                if ((replay_stage_mask[abs] & (1 << i)) == 0) cursor.disable(i);
            }
            // 原版：落在首个可用项上（move -1/+1 跳过禁用）
            cursor.move(-1);
            cursor.move(1);
            if (replay_spell_practice[abs]) {
                replay_stage_choice = cursor.index;
                set_sub(3);
                replay_ui_flags |= 4u;
                sfx(0x32);
                if (on_screen_fade) {/* BGM fade 0x43c470 */}
                events.push_back({"bgm-fade", 0});
            }
        }
        break;
    }
    case 3:
        if (timer == 2) transition_effect();
        if (timer > 0x1f && (replay_ui_flags & 8u) != 0) {
            set_screen(kFinished);  // 原版 set_screen(2) 后写 DAT_004c17c4=0xd
            char path[512];
            std::snprintf(path, sizeof(path), "%s/th16_%02d.rpy", replay_directory.c_str(), replay_choice + 1);
            pending_replay_path = path;
            {
                ReplayFile probe;
                if (!probe.open_file(path)) {
                    std::snprintf(path, sizeof(path), "%s/replay/th16_%02d.rpy", replay_directory.c_str(), replay_choice + 1);
                    pending_replay_path = path;
                }
            }
            last_replay_slot = replay_choice;
            globals->scene_request = 0xd;
            globals->replay_slot = replay_stage_choice;  // DAT_004a57c8 路径相关；阶段选择另见 pending
            events.push_back({"scene", 0xd});
            events.push_back({"replay-play", replay_choice});
        }
        break;
    case 4:
        if (timer > 0xe) {
            const i32 prev = cursor.index;
            const u32 edge = input.pressed | input.repeat;
            if (edge & InputState::kUp) cursor.move(-1);
            if (edge & InputState::kDown) cursor.move(1);
            if (cursor.index != prev) sfx(10);
            if (input.pressed & InputState::kCancelMask) {
                cursor.pop();
                cursor.count = 0x19;
                set_sub(2);
                sfx(9);
                break;
            }
            if (input.pressed & InputState::kConfirmMask) {
                replay_stage_choice = cursor.index;
                set_sub(3);
                replay_ui_flags |= 4u;
                sfx(0x32);
                events.push_back({"bgm-fade", 0});
            }
        }
        break;
    case 5:
        if (timer > 5 && (replay_ui_flags & 8u) != 0) {
            dismiss(anm, 0x6c);
            dismiss(anm, kLeafEmitter);
            set_screen(kMainMenu);
            cursor.pop();
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

void TitleScene::close_help() {
    delete help;
    help = nullptr;
}

// 0x4545a0：Manual → 0x42ea30 HelpScreen；关闭后回主菜单
void TitleScene::update_manual(AnmManager& anm, const InputState& input) {
    switch (sub) {
    case 0: {
        if (vm[kLeafEmitter] == 0 && ascii_file) spawn_into(anm, kLeafEmitter, *ascii_file, kLeafEmitterScript);
        if (title_file) spawn_into(anm, 0x72, *title_file, 0x72);  // +0x488
        close_help();
        help = new HelpScreen();
        help->anm = load_help_anm ? load_help_anm() : nullptr;
        help->scroll = 112.0f;  // 0x43000000
        set_sub(1);
        events.push_back({"manual", 0});
        break;
    }
    case 1:
        if (help != nullptr) {
            if (help->anm == nullptr && load_help_anm) help->anm = load_help_anm();
            if (help->anm != nullptr) {
                help->update(anm, input, 1.0f);
                for (i32 id : help->sfx_queue) sfx(id);
                help->sfx_queue.clear();
            } else if (input.pressed & InputState::kCancelMask) {
                // help.anm 未装上时允许取消退出（harness 无 help 资源仍可回归）
                sfx(9);
                help->closed = true;
            }
            if (help->closed) {
                close_help();
                dismiss(anm, kLeafEmitter);
                dismiss(anm, 0x72);
                set_screen(kMainMenu);
                cursor.pop();
                events.push_back({"screen", screen});
            }
        }
        break;
    default:
        break;
    }
}

void TitleScene::parse_music_cmt(const char* text, size_t len) {
    music_tracks.clear();
    if (text == nullptr || len == 0) return;
    size_t i = 0;
    while (i < len) {
        size_t line = i;
        while (i < len && text[i] != '\n' && text[i] != '\r') ++i;
        if (i > line && text[line] == '@') {
            std::string path(text + line + 1, text + i);
            while (!path.empty() && (path.back() == ' ' || path.back() == '\t')) path.pop_back();
            if (!path.empty()) music_tracks.push_back(path);
        }
        if (i < len && text[i] == '\r') ++i;
        if (i < len && text[i] == '\n') ++i;
    }
}

// 0x4546f0：Music Room（曲目列表导航 / 确定播 BGM / 取消回主菜单并恢复 th16_01）
void TitleScene::update_music(AnmManager& anm, const InputState& input) {
    switch (sub) {
    case 0: {
        if (vm[kLeafEmitter] == 0 && ascii_file) spawn_into(anm, kLeafEmitter, *ascii_file, kLeafEmitterScript);
        if (title_file) spawn_into(anm, 0x6e, *title_file, 0x6e);  // +0x478
        if (music_tracks.empty()) {
            for (int n = 1; n <= 17; ++n) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "bgm/th16_%02d", n);
                music_tracks.emplace_back(buf);
            }
        }
        cursor.count = i32(music_tracks.size());
        cursor.wrap = true;
        cursor.set(0);
        music_scroll = 0;
        set_sub(1);
        events.push_back({"music", i32(music_tracks.size())});
        break;
    }
    case 1:
        if (timer > 4) set_sub(2);
        break;
    case 2: {
        const i32 prev = cursor.index;
        const u32 edge = input.pressed | input.repeat;
        if (edge & InputState::kUp) cursor.move(-1);
        if (edge & InputState::kDown) cursor.move(1);
        if (cursor.index != prev) {
            sfx(10);
            if (cursor.index < music_scroll) music_scroll = cursor.index;
            else if (cursor.index >= music_scroll + 10) music_scroll = cursor.index - 9;
        }
        if (input.pressed & InputState::kConfirmMask) {
            if (cursor.index >= 0 && cursor.index < i32(music_tracks.size())) {
                events.push_back({"bgm", cursor.index});
                // 路径事件：用正数下标；具体名由 harness 侧查 music_tracks
            }
        }
        if (input.pressed & InputState::kCancelMask) {
            sfx(9);
            set_sub(3);
        }
        break;
    }
    case 3:
        if (timer > 9) {
            dismiss(anm, 0x6e);
            dismiss(anm, kLeafEmitter);
            set_screen(kMainMenu);
            cursor.pop();
            events.push_back({"bgm-default", 1});  // FUN_0043c370(0,"th16_01")
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

// 0x4553d0：符卡练习分组选择（←/→ 机体副光标；确定 → 画面 0x12）
void TitleScene::update_spell_practice(AnmManager& anm, const InputState& input) {
    switch (sub) {
    case 0: {
        if (vm[kLeafEmitter] == 0 && ascii_file) spawn_into(anm, kLeafEmitter, *ascii_file, kLeafEmitterScript);
        cursor.count = 7;
        cursor.wrap = true;
        cursor.set(0);
        spell_shot_cursor.count = 4;
        spell_shot_cursor.wrap = true;
        spell_shot_cursor.set(globals->shot);
        if (title_file) {
            if (vm[0xd7] == 0) spawn_into(anm, 0xd7, *title_file, 0xd7);   // +0x61c
            if (vm[0x11c] == 0) spawn_into(anm, 0x11c, *title_file, 0x11c); // +0x730
            spawn_into(anm, 0x71, *title_file, 0x71);  // +0x484 底图
        }
        set_sub(1);
        events.push_back({"spell-practice", 0});
        [[fallthrough]];
    }
    case 1:
        if (timer > 10) {
            set_sub(2);
            if (vm[0xd7]) {
                anm.interrupt_now(vm[0xd7], 3);
                anm.interrupt(vm[0xd7], to_short(cursor.index + 7));
            }
            if (vm[0x11c]) {
                anm.interrupt_now(vm[0x11c], 3);
                anm.interrupt(vm[0x11c], to_short(spell_shot_cursor.index + 7));
            }
        }
        break;
    case 2: {
        const i32 prev = cursor.index;
        const i32 prev_shot = spell_shot_cursor.index;
        const u32 edge = input.pressed | input.repeat;
        if (edge & InputState::kUp) cursor.move(-1);
        if (edge & InputState::kDown) cursor.move(1);
        if (edge & InputState::kLeft) spell_shot_cursor.move(-1);
        if (edge & InputState::kRight) spell_shot_cursor.move(1);
        if (cursor.index != prev) {
            sfx(10);
            if (vm[0xd7]) {
                anm.interrupt_now(vm[0xd7], 3);
                anm.interrupt(vm[0xd7], to_short(cursor.index + 7));
            }
        }
        if (spell_shot_cursor.index != prev_shot) {
            sfx(10);
            if (vm[0x11c]) {
                anm.interrupt_now(vm[0x11c], 3);
                anm.interrupt(vm[0x11c], to_short(spell_shot_cursor.index + 7));
            }
            // 0x456a20：DAT_004a57a4 = 机体光标；DAT_004a57a8 = 0；DAT_004a57ac = 4（土用）
            globals->shot = spell_shot_cursor.index;
            globals->shot_aux = 0;
            globals->season = 4;
        }
        if (input.pressed & InputState::kCancelMask) {
            set_sub(4);
            sfx(9);
            break;
        }
        if (input.pressed & InputState::kConfirmMask) {
            if (vm[0xd7]) anm.interrupt(vm[0xd7], 6);
            set_sub(3);
            sfx(7);
            spell_group = cursor.index;
        }
        break;
    }
    case 3:
        if (timer > 0x13) {
            dismiss(anm, 0x71);
            set_screen(kSpellList);
            cursor.push();
            cursor.set(0);
            events.push_back({"screen", screen});
        }
        break;
    case 4:
        if (timer > 5) {
            dismiss(anm, 0x71);
            dismiss(anm, 0xd7);
            dismiss(anm, 0x11c);
            dismiss(anm, kLeafEmitter);
            set_screen(kMainMenu);
            cursor.pop();
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

// 0x452330：Player Data（难度←→ / 机体↑↓ / 翻页确定；取消回主菜单）
void TitleScene::update_score(AnmManager& anm, const InputState& input) {
    switch (sub) {
    case 0: {
        cursor.count = 4;
        cursor.wrap = true;
        cursor.set(0);
        score_shot_cursor.count = 5;
        score_shot_cursor.wrap = true;
        score_shot_cursor.set(1);
        score_page_cursor.count = 2;  // 0 列表关 / 1 翻开
        score_page_cursor.wrap = false;
        score_page_cursor.set(0);
        if (vm[kLeafEmitter] == 0 && ascii_file) spawn_into(anm, kLeafEmitter, *ascii_file, kLeafEmitterScript);
        if (title_file) spawn_into(anm, 0x6d, *title_file, 0x6d);  // +0x474
        set_sub(1);
        events.push_back({"score", 0});
        [[fallthrough]];
    }
    case 1:
        if (timer > 6) set_sub(2);
        break;
    case 2: {
        const i32 prev_diff = cursor.index;
        const i32 prev_shot = score_shot_cursor.index;
        const u32 edge = input.pressed | input.repeat;
        if (edge & InputState::kLeft) cursor.move(-1);
        if (edge & InputState::kRight) cursor.move(1);
        if (edge & InputState::kUp) score_shot_cursor.move(-1);
        if (edge & InputState::kDown) score_shot_cursor.move(1);
        if (cursor.index != prev_diff || score_shot_cursor.index != prev_shot) sfx(10);
        if (input.pressed & InputState::kConfirmMask) {
            // 原版：翻页光标在 0↔非0 间切换并刷成绩行；此处只发事件
            score_page_cursor.set(score_page_cursor.index == 0 ? 1 : 0);
            sfx(7);
            events.push_back({"score-page", score_page_cursor.index});
        }
        if (input.pressed & InputState::kCancelMask) {
            set_sub(3);
            sfx(9);
        }
        break;
    }
    case 3:
        if (timer > 5) {
            dismiss(anm, 0x6d);
            dismiss(anm, kLeafEmitter);
            set_screen(kMainMenu);
            cursor.pop();
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

// 0x455d50：符卡列表（count 4；确定开战；取消 → 画面 0x13）
void TitleScene::update_spell_list(AnmManager& anm, const InputState& input) {
    switch (sub) {
    case 0:
        cursor.count = 4;
        cursor.wrap = true;
        cursor.set(0);
        if (title_file && vm[0xd9] == 0) spawn_into(anm, 0xd9, *title_file, 0xd9);  // +0x624
        set_sub(1);
        events.push_back({"spell-list", spell_group});
        [[fallthrough]];
    case 1:
        if (timer > 10) {
            set_sub(2);
            if (vm[0xd9]) {
                anm.interrupt_now(vm[0xd9], 3);
                anm.interrupt(vm[0xd9], to_short(cursor.index + 7));
            }
        }
        break;
    case 2: {
        const i32 prev = cursor.index;
        const u32 edge = input.pressed | input.repeat;
        if (edge & InputState::kUp) cursor.move(-1);
        if (edge & InputState::kDown) cursor.move(1);
        if (cursor.index != prev) {
            sfx(10);
            if (vm[0xd9]) {
                anm.interrupt_now(vm[0xd9], 3);
                anm.interrupt(vm[0xd9], to_short(cursor.index + 7));
            }
        }
        // ←/→ 机体副光标（0x455790）
        const i32 prev_shot = spell_shot_cursor.index;
        if (edge & InputState::kLeft) spell_shot_cursor.move(-1);
        if (edge & InputState::kRight) spell_shot_cursor.move(1);
        if (spell_shot_cursor.index != prev_shot) {
            sfx(10);
            globals->shot = spell_shot_cursor.index;
        }
        if (input.pressed & InputState::kCancelMask) {
            set_sub(4);
            sfx(9);
            break;
        }
        if (input.pressed & InputState::kConfirmMask) {
            if (vm[0xd9]) anm.interrupt(vm[0xd9], 6);
            set_sub(3);
            sfx(7);
            sfx(0x32);
            events.push_back({"bgm-fade", 0});
        }
        break;
    }
    case 3:
        if (timer == 10) transition_effect();
        if (timer > 0x27) {
            // 符卡练习开战：场景 7，mode 已在主菜单置 2
            globals->stage = spell_group + 1;
            globals->stage_copy = globals->stage;
            globals->difficulty = cursor.index;  // 列表项暂作难度/条目；精确表见 0x5dd0
            globals->scene_request = kSceneGame;
            events.push_back({"scene", kSceneGame});
            set_screen(kFinished);
        }
        break;
    case 4:
        if (timer > 5) {
            dismiss(anm, 0xd9);
            set_screen(kSpellShot);
            cursor.pop();
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

// 0x456a20：符卡机体/条目页（count 5；取消回列表 0x12）
void TitleScene::update_spell_shot(AnmManager& anm, const InputState& input) {
    (void)anm;
    switch (sub) {
    case 0:
        cursor.count = 5;
        cursor.wrap = true;
        cursor.set(0);
        set_sub(1);
        events.push_back({"spell-shot", spell_group});
        [[fallthrough]];
    case 1:
        if (timer > 10) set_sub(2);
        break;
    case 2: {
        const i32 prev = cursor.index;
        const u32 edge = input.pressed | input.repeat;
        if (edge & InputState::kUp) cursor.move(-1);
        if (edge & InputState::kDown) cursor.move(1);
        if (cursor.index != prev) sfx(10);
        const i32 prev_shot = spell_shot_cursor.index;
        if (edge & InputState::kLeft) spell_shot_cursor.move(-1);
        if (edge & InputState::kRight) spell_shot_cursor.move(1);
        if (spell_shot_cursor.index != prev_shot) {
            sfx(10);
            globals->shot = spell_shot_cursor.index;
        }
        if (input.pressed & InputState::kCancelMask) {
            set_sub(4);
            sfx(9);
            break;
        }
        if (input.pressed & InputState::kConfirmMask) {
            set_sub(3);
            sfx(7);
            if (spell_group == 6) {
                sfx(0x32);
                events.push_back({"bgm-fade", 0});
            }
        }
        break;
    }
    case 3:
        if (spell_group == 6) {
            if (timer == 10) transition_effect();
            if (timer > 0x27) {
                globals->stage = spell_group + 1;
                globals->stage_copy = globals->stage;
                globals->scene_request = kSceneGame;
                events.push_back({"scene", kSceneGame});
                set_screen(kFinished);
            }
        } else if (timer > 0xd) {
            // 非 Extra 组：进更深页 0x14（未接）→ 暂回列表
            set_screen(kSpellList);
            cursor.push();
            events.push_back({"screen", screen});
        }
        break;
    case 4:
        if (timer > 5) {
            set_screen(kSpellList);
            cursor.pop();
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

// 0x44e930：Key Config（7 项：5 键位 + Default + Quit）；捕获写入 0x44f710
void TitleScene::update_key_config(AnmManager& anm, const InputState& input) {
    switch (sub) {
    case 0:
        cursor.count = 7;
        cursor.wrap = true;
        cursor.set(0);
        key_bind_slot = -1;
        pending_scancode = -1;
        if (title_file) spawn_into(anm, 2, *title_file, 2);  // +0x2c8 面板脚本 2
        set_sub(1);
        events.push_back({"key-config", 0});
        [[fallthrough]];
    case 1:
        if (timer > 6) {
            set_sub(2);
            if (vm[2]) {
                anm.interrupt_now(vm[2], 3);
                anm.interrupt(vm[2], cursor.index + 0x11);
            }
        }
        break;
    case 2: {
        const i32 prev = cursor.index;
        const u32 edge = input.pressed | input.repeat;
        if (edge & InputState::kUp) cursor.move(-1);
        if (edge & InputState::kDown) cursor.move(1);
        if (cursor.index != prev) {
            sfx(10);
            if (vm[2]) {
                anm.interrupt_now(vm[2], 3);
                anm.interrupt(vm[2], to_short(cursor.index + 7));
            }
        }
        // 取消仅在 Quit(6) 上退出；否则跳到 Quit（与 Option 同型）
        if (input.pressed & InputState::kCancelMask) {
            if (cursor.index != 6) {
                cursor.set(6);
                sfx(9);
                if (vm[2]) {
                    anm.interrupt_now(vm[2], 3);
                    anm.interrupt(vm[2], to_short(cursor.index + 7));
                }
                break;
            }
            if (vm[2]) anm.interrupt(vm[2], 6);
            sfx(9);
            set_sub(4);
            break;
        }
        if (input.pressed & InputState::kConfirmMask) {
            if (cursor.index == 5) {
                if (globals) globals->reset_key_binds();
                sfx(7);
                events.push_back({"key-default", 0});
            } else if (cursor.index == 6) {
                if (vm[2]) anm.interrupt(vm[2], 6);
                sfx(9);
                set_sub(4);
            } else {
                // 项 0..4：进入捕获（0x44f710 等下一键；宿主写 pending_scancode）
                key_bind_slot = cursor.index;
                pending_scancode = -1;
                set_sub(3);
                events.push_back({"key-bind-wait", key_bind_slot});
            }
        }
        break;
    }
    case 3:
        // 捕获中：取消放弃；pending_scancode≥0 → 0x44f710 写入后回列表
        if (input.pressed & InputState::kCancelMask) {
            key_bind_slot = -1;
            pending_scancode = -1;
            sfx(9);
            set_sub(2);
            events.push_back({"key-bind-cancel", 0});
            break;
        }
        if (pending_scancode >= 0 && key_bind_slot >= 0 && globals != nullptr) {
            const i32 slot = key_bind_slot;
            const i32 code = pending_scancode;
            globals->assign_key_bind(slot, i16(code));
            events.push_back({"key-bind", slot});
            events.push_back({"key-scancode", code});
            sfx(7);
            key_bind_slot = -1;
            pending_scancode = -1;
            set_sub(2);
        }
        break;
    case 4:
        if (timer > 9) {
            dismiss(anm, 2);
            set_screen(kOption);
            cursor.pop();
            events.push_back({"screen", screen});
        }
        break;
    default:
        break;
    }
}

// 待还原画面（若仍有未接线的标题页）
void TitleScene::update_stub(AnmManager& anm, const InputState& input) {
    (void)anm;
    if (sub == 0) {
        set_sub(1);
        events.push_back({"stub", screen});
    }
    if (input.pressed & InputState::kCancelMask) {
        sfx(9);
        set_screen(kMainMenu);
        cursor.pop();
        events.push_back({"screen", screen});
    }
}

}  // namespace th16
