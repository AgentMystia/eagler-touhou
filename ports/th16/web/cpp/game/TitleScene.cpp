#include "TitleScene.hpp"

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

// 0x44af80
void TitleScene::update(AnmManager& anm, const InputState& input) {
    if (title_file == nullptr) return;
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

// 待还原画面（Option / Player Data / Replay / Music Room / Manual / Spell Practice）：取消回主菜单
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
