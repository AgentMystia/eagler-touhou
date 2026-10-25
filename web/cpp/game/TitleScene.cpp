#include "TitleScene.hpp"

namespace th13 {

TitleScene::TitleScene(SessionState& p, TitleSelectionSettings& choice, AnmManager& a,
                       TitleScenePlatform& h, FrameScheduler& schedule, TitleSceneResources bank)
    : progress(p), settings(choice), animations(a), host(h), scheduler(schedule), resources(bank),
      main(state, p, choice, a, bank), difficulty(state, p, choice, a, bank),
      character(state, p, choice, a, *this, bank),
      stage(state, p, choice, a, *this, bank), replay(state, a, h, bank),
      option(state, a, h, option_volumes, bank), music_room(state, h), manual(state, h),
      frame(state, p, *this, schedule, a) {
    state.screen = TitleScreen::Initialize;
    state.flags = 0;
    main.play_sound = [this](i32 id) { return host.sound(id); };
    main.extra_unlocked = [this](i32 character) { return host.extra_unlocked(character); };
    difficulty.play_sound = [this](i32 id) { return host.sound(id); };
    replay.play_sound = [this](i32 id) { return host.sound(id); };
    option.play_sound = [this](i32 id) { return host.sound(id); };
    music_room.play_sound = [this](i32 id) { return host.sound(id); };
    stage_text_draw.owner = this;
    stage_text_draw.enabled = true;
    stage_text_draw.run = [](void* owner) {
        auto& scene = *static_cast<TitleScene*>(owner);
        return scene.check(scene.stage.draw_texts(scene.host), scene.stage.error) ? 1 : 5;
    };
    check(scheduler.add(stage_text_draw, FramePass::Draw, 2) >= 0,
          "Practice stage text callback registration failed");
}

TitleScene::~TitleScene() {
    scheduler.remove(stage_text_draw);
    for (auto& handle : state.handles) animations.retire(handle);
    animations.retire(state.version_bar);
    animations.retire(state.start_effect);
    animations.retire(state.replay_ascii_root);  // [R3-M] ascii s19 粒子根
    // 析构 bank 清单（蓝图 §五-12）：title=16 / title_v=17 / help=19（help 补漏）。
    for (const i32 bank : {resources.title, resources.version, resources.help})
        animations.retire_resource(bank);
}

bool TitleScene::check(bool ok, const std::string& why) {
    if (!ok && error.empty()) error = why.empty() ? "Title scene operation failed" : why;
    return ok;
}

bool TitleScene::clear_title_overlay() { return host.clear_title_overlay(); }
bool TitleScene::queue_title_music(i32 code, i32 value, const std::string& name) {
    return host.queue_title_music(code, value, name);
}
bool TitleScene::clear_current_wave() { return host.clear_current_wave(); }
bool TitleScene::title_exit(i32 value) { return host.title_exit(value); }
bool TitleScene::fade_out_title_music() { return host.fade_out_title_music(); }

bool TitleScene::sound(i32 id) { return host.sound(id); }

bool TitleScene::begin_transition(u32& animation) {
    // 开始游戏转场特效（effect bank script 0；th15 用 create(8,0)）。
    animation = animations.create(8, 0, -1, 0);
    return animation != 0;
}

bool TitleScene::start_game(i32 stage) {
    // destination=13：进入对局（原版 supervisor 语义，数值经 ApplicationState 隔离）。
    return host.start_game(stage);
}

bool TitleScene::character_cleared(i32 character, i32 difficulty) {
    // [r4-M] clear-tag 可见门数据源转发（TitleMenuServices → 宿主持久进度）。
    return host.character_cleared(character, difficulty);
}

bool TitleScene::extra_unlocked(i32 character) {
    // Extra 行解锁门转发（0x44c570 case0 +0x5c40 bit1 → 宿主 FileStore 持久位）。
    return host.extra_unlocked(character);
}

bool TitleScene::practice_stage_played(i32 character, i32 difficulty, i32 stage) {
    return host.practice_stage_played(character, difficulty, stage);
}

void TitleScene::advance_main_menu_selection() { main.advance_selection(); }

bool TitleScene::draw_title_replay_texts() {
    // Replay 屏动态文字：TitleFrame Draw 回调（优先级 2，先于全部 ANM 层）逐帧
    // 入队，AsciiFrame（Draw 78）同帧消费——原版 supervisor 0x452c30 的 ASCII
    // 队列通道等价路径。
    return check(replay.draw_texts(host), replay.error);
}

bool TitleScene::draw_title_music_room_texts() {
    // [r4-M2] 音楽室列表/注释：同 Replay 文本队列通道（0x4554f0 值域）。
    return check(music_room.draw_texts(host), music_room.error);
}

bool TitleScene::restore_practice_selection() {
    if (!returns_to_practice_selection(progress, 1, false))
        return check(false, "Invalid Practice return session");
    settings.configured_difficulty = progress.difficulty;
    settings.preferred_character = progress.character;
    settings.practice_stage = progress.starting_stage - 1;
    state.menu.depth = state.menu.disabled_count = 0;
    state.menu.count = kTitleMainEntries;
    state.menu.select(i32(TitleMainEntry::PracticeStart));
    state.menu.push();
    state.menu.count = 4;
    state.menu.select(progress.difficulty);
    state.menu.push();
    state.menu.count = kTitleCharacterRows;
    state.menu.select(progress.character + kTitleCharacterFirst);
    state.menu.push();
    state.menu.count = 6;
    state.menu.select(settings.practice_stage);
    state.change_screen(TitleScreen::Stage);
    return true;
}

bool TitleScene::update_title_menu(TitleScreen screen, const TitleFrameInput& value) {
    input = value;
    switch (screen) {
    case TitleScreen::Main:
        return check(main.update(value.pressed, value.repeated), main.error);
    case TitleScreen::Option:
        // 屏值 3（0x44c570 case 3 → 0x44dd00）：Option 屏驱动。
        return check(option.update(value.pressed, value.repeated), option.error);
    case TitleScreen::Difficulty:
        return check(difficulty.update(value.pressed, value.repeated), difficulty.error);
    case TitleScreen::Character:
        return check(character.update(value.pressed, value.repeated), character.error);
    case TitleScreen::Stage:
        return check(stage.update(value.pressed, value.repeated), stage.error);
    case TitleScreen::Replay:
        return check(replay.update(value.pressed, value.repeated), replay.error);
    case TitleScreen::MusicRoom:
        // 屏值 13（0x44c570 case d → 0x4554f0）：音楽室驱动。
        return check(music_room.update(value.pressed, value.repeated), music_room.error);
    case TitleScreen::Manual:
        // 屏值 16（0x44c570 case 10 → 0x455330）：マニュアル supervisor wrapper。
        return check(manual.update(value.pressed, value.repeated), manual.error);
    default:
        return check(false, "Unsupported title scene menu");
    }
}

}  // namespace th13
