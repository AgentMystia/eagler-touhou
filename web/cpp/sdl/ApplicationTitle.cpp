// 标题期平台服务（th15 ApplicationTitle.cpp 裁剪：音乐请求先落桩）。
#include "ApplicationState.hpp"

namespace th13::sdl {

bool ApplicationState::sound(i32 id) { return audio_device.enqueue(id) || fail(audio_device.error); }

bool ApplicationState::hud_text(const HudTextDraw& value) {
    return captions.enqueue(value, 0, value.style.shadow) || fail(captions.error);
}

bool ApplicationState::clear_title_overlay() {
    // 过场覆盖层（th15 transition overlay）随 Stage 转场分期接入；标题无残留。
    return true;
}

bool ApplicationState::queue_title_music(i32 code, i32 value, const std::string& name) {
    return audio_device.queue_music(code, value, name) || fail(audio_device.error);
}

bool ApplicationState::clear_current_wave() { return true; }

bool ApplicationState::title_exit(i32 value) {
    // StartGame 与 Quit 共享 TitleScreen 值 2：开局请求已排队时不覆盖（th15 同款守卫）。
    if (pending_destination == i32(Destination::Run)) return true;
    pending_destination = value;
    return true;
}

bool ApplicationState::fade_out_title_music() {
    return audio_device.queue_music(5, 0, "FadeOut") || fail(audio_device.error);
}

bool ApplicationState::begin_transition(u32& animation) {
    animation = animations.create(8, 0, -1, 0);  // effect bank 转场特效
    return animation != 0;
}

bool ApplicationState::start_game(i32 stage) {
    selected_stage = stage;
    progress.new_run = true;
    pending_destination = i32(Destination::Run);
    return true;
}

}  // namespace th13::sdl
