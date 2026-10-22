#include "TitleScene.hpp"

namespace th13 {

TitleScene::TitleScene(SessionState& p, TitleSelectionSettings& choice, AnmManager& a,
                       TitleScenePlatform& h, FrameScheduler& schedule, TitleSceneResources bank)
    : progress(p), settings(choice), animations(a), host(h), scheduler(schedule), resources(bank),
      main(state, p, choice, a, bank), difficulty(state, p, choice, a, bank),
      character(state, p, choice, a, *this, bank), frame(state, p, *this, schedule) {
    state.screen = TitleScreen::Initialize;
    state.flags = 0;
    main.play_sound = [this](i32 id) { return host.sound(id); };
    difficulty.play_sound = [this](i32 id) { return host.sound(id); };
}

TitleScene::~TitleScene() {
    for (auto& handle : state.handles) animations.retire(handle);
    animations.retire(state.version_bar);
    animations.retire(state.start_effect);
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

bool TitleScene::update_title_menu(TitleScreen screen, const TitleFrameInput& value) {
    input = value;
    switch (screen) {
    case TitleScreen::Main:
        return check(main.update(value.pressed, value.repeated), main.error);
    case TitleScreen::Difficulty:
        return check(difficulty.update(value.pressed, value.repeated), difficulty.error);
    case TitleScreen::Character:
        return check(character.update(value.pressed, value.repeated), character.error);
    default:
        return check(false, "Unsupported title scene menu");
    }
}

}  // namespace th13
