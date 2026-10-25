#include "RunPause.hpp"

namespace th13 {

struct RunPause::Services final : PauseMenuServices {
    RunPause& owner;
    explicit Services(RunPause& p) : owner(p) {}
    bool sound(i32 id) override { return owner.sound(id); }
    bool finish_selection(i32 entry) override { return owner.finish_selection(entry); }
};

RunPause::RunPause(SessionState& p, AnmManager& a, AnmEnvironment& e, RunPausePlatform& h,
                   FrameScheduler& f, i32 bank, i32 text_bank)
    : services(std::make_unique<Services>(*this)), progress(p), animations(a), environment(e),
      platform(h), front(bank), text(text_bank), menu(state, a, *services), frame(state, p, menu, f) {}

RunPause::~RunPause() { frame.enable(false); }

bool RunPause::fail(const std::string& why) {
    if (error.empty()) error = why.empty() ? "Run pause unavailable" : why;
    return false;
}

bool RunPause::sound(i32 id) { return platform.sound(id); }

bool RunPause::finish_selection(i32 entry) {
    // 收场：恢复对局速率与场景旗标，冻结解除；重开/回标题交由平台请求 destination。
    progress.rate = state.saved_rate;
    progress.scene_flags &= ~0x10u;
    environment.paused = false;
    if (!animations.retire(state.menu_animation) ||
        !animations.retire(state.snapshot_animation)) return fail(animations.error);
    state.menu_animation = 0;
    switch (entry) {
    case 0:
        return platform.resume_music() || fail("Pause music resume failed");
    case 1:
        return platform.return_to_title() || fail("Pause title destination failed");
    case 4:
        return platform.restart_run() || fail("Pause restart destination failed");
    default:
        return fail("Pause selection outside original range");
    }
}

bool RunPause::prepare() {
    if (!error.empty() || !frame.error.empty()) return fail(frame.error);
    if (!animations.resource(front)) return fail("Run pause front resource unavailable");
    if (!animations.resource(text)) return fail("Run pause text resource unavailable");
    frame.enable(true);
    return true;
}

bool RunPause::open(PauseEntrance entrance) {
    if (!error.empty()) return false;
    // 激活序列（th15 PauseActivation 裁剪）：菜单视觉 → 音频挂起 → 表现冻结。
    state.select(entrance == PauseEntrance::GameOver ? PauseScreen::GameOver : PauseScreen::Pause);
    progress.scene_flags |= 0x10;
    state.front_bank = front;
    state.practice = progress.practice_mode != 0;
    if (entrance == PauseEntrance::GameOver) state.select_phase(0);
    if (!animations.retire(state.menu_animation) ||
        !animations.retire(state.snapshot_animation)) return fail(animations.error);
    // overlay 菜单 = create(ordering 4) + 清 0xc000 渲染位（th15 create_overlay 同义）。
    state.snapshot_animation = animations.create(text, kPauseBackdropScript, -1, 4);
    if (!state.snapshot_animation) return fail(animations.error);
    const i32 script = entrance == PauseEntrance::GameOver ? kGameOverMenuScript
                       : state.practice ? kPracticePauseMenuScript : kPauseMenuScript;
    state.menu_animation = animations.create(front, script, -1, 4);
    if (auto* vm = animations.registry.find(state.menu_animation))
        vm->visual.render_flags &= ~0xc000u;
    if (!state.menu_animation) return fail(animations.error);
    state.saved_rate = progress.rate;
    progress.rate = 1;
    environment.paused = true;
    if (!platform.suspend_music()) return fail("Pause music suspend failed");
    return animations.interrupt(state.menu_animation, 3) || fail(animations.error);
}

}  // namespace th13
