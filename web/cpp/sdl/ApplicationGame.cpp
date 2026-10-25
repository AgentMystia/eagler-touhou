// 对局期平台服务（th15 ApplicationGame.cpp 裁剪：life_hud/bomb_hud/game_over
// 保留，battle/敌机/符卡回调随分期接入）。
#include "ApplicationState.hpp"

namespace th13::sdl {

bool ApplicationState::life_hud(i32 stock, i32 pieces) {
    return run && run->hud.life(stock, pieces) || fail(run ? run->hud.error : "Life HUD has no scene");
}

bool ApplicationState::bomb_hud(i32 stock, i32 pieces) {
    return run && run->hud.bombs(stock, pieces) ||
           fail(run ? run->hud.error : "Bomb HUD has no scene");
}

bool ApplicationState::game_over() {
    return pause && pause->open(PauseEntrance::GameOver) ||
           fail(pause ? pause->error : "Game-over menu has no owner");
}

bool ApplicationState::request_pause() {
    return pause && pause->open(PauseEntrance::Pause) || fail(pause ? pause->error : "Pause has no owner");
}

bool ApplicationState::request_game_over() {
    return game_over();  // 回放会话同语义：无终局豁免（0x448200 死亡照常进 GameOver）
}

bool ApplicationState::hud_sound(i32 id) { return sound(id); }

bool ApplicationState::suspend_music() {
    audio_device.pause_music(true);
    return true;
}

bool ApplicationState::resume_music() {
    audio_device.pause_music(false);
    return true;
}

bool ApplicationState::restart_run() {
    return start_game(progress.starting_stage);
}

bool ApplicationState::return_to_title() {
    pending_destination = i32(Destination::Title);
    return true;
}

}  // namespace th13::sdl
