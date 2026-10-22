#include "TitleFrame.hpp"

namespace th13 {

TitleFrame::TitleFrame(TitleState& s, SessionState& p, TitleFrameServices& h, FrameScheduler& f)
    : state(s), progress(p), host(h), scheduler(f) {
    callbacks[0].owner = callbacks[1].owner = this;
    callbacks[0].enabled = true;
    callbacks[1].enabled = false;
    callbacks[0].run = [](void* p) {
        auto& frame = *static_cast<TitleFrame*>(p);
        return frame.update(frame.input) ? i32(FrameAction::Continue) : i32(FrameAction::Error);
    };
    callbacks[1].run = [](void* p) {
        auto& frame = *static_cast<TitleFrame*>(p);
        return frame.drawing() ? i32(FrameAction::Continue) : i32(FrameAction::Error);
    };
    if (scheduler.add(callbacks[0], FramePass::Update, 6) < 0 ||
        scheduler.add(callbacks[1], FramePass::Draw, 66) < 0)
        check(false, "Title callback registration failed");
}

TitleFrame::~TitleFrame() {
    for (auto& callback : callbacks) scheduler.remove(callback);
}

bool TitleFrame::check(bool ok, const char* why) {
    if (!ok && error.empty()) error = why;
    return ok;
}

bool TitleFrame::menu() { return check(host.update_title_menu(state.screen, input), "Title menu update failed"); }

bool TitleFrame::initialize() {
    if (!check(host.clear_title_overlay(), "Title text overlay release failed")) return false;
    const i32 reason = state.return_reason;
    auto main = [&]() {
        state.change_screen(TitleScreen::Main);
        state.return_reason = 1;
        callbacks[1].enabled = true;
        return menu();
    };
    if (reason == 0) {
        // 冷启动（真机 boot）：无提示幕，走完整时序——开场 logo 幕 180 帧后
        // 标题展开、菜单 intro 180 帧插值（oracle g181/g361/g372 锚点）。
        state.flags &= ~2u;
        return main();
    }
    // 自对局/回放返回（return_reason 值域待 supervisor 反汇编锚定，蓝图 §五-9）：
    // 菜单直接展开（背景复位根 script95、无 180 帧等待——推定，待 oracle）。
    state.flags |= 2u;
    return main();
}

bool TitleFrame::update(const TitleFrameInput& value) {
    if (!error.empty()) return false;
    input = value;
    switch (i32(state.screen)) {
    case 0:  // Initialize
        if (!initialize()) return false;
        break;
    case 1:  // Main
        callbacks[1].enabled = true;
        if (!menu()) return false;
        break;
    case 2:  // Quit / StartGame
        if (state.screen == TitleScreen::Quit) {
            if (!check(host.title_exit(3), "Title exit destination failed")) return false;
            if (!check(host.fade_out_title_music(), "Title exit music fade failed")) return false;
        }
        break;
    case 6:  // Difficulty
    case 7:  // Character
        if (!menu()) return false;
        break;
    default:
        break;
    }
    state.age.tick(&progress.rate);
    return true;
}

}  // namespace th13
