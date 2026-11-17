#include "GameManager.hpp"

#include "AnmManager.hpp"
#include "GameGlobals.hpp"

namespace th16 {

void GameManager::first_frame(AnmManager& manager, GameGlobals& globals) {
    // 0x42dc50：清重试位；0x40add0 建面 VM、启用关卡链；帧计数清零；（其余管理器复位 / BGM 待接入）
    flags &= ~kRetry;
    if (stage_live) stage.start(manager);
    if (player_live) player.reset(manager, globals);   // 0x441740
    globals.game_frame = 0;
    globals.stage_frame = 0;
}

bool GameManager::update(AnmManager& manager, GameGlobals& globals, float game_speed) {
    // 0x42d7b0
    if ((flags & kStageClear) != 0) {
        // 过关流程（+0xb0 计数、0x45c900 结算对象、下一关请求）随 ECL 接入
        ++end_counter;
    }
    if (timer.current == 0) {
        if ((flags & kLoadFailed) != 0) return true;
        first_frame(manager, globals);
    }
    if ((flags & kLoading) != 0) {
        flags |= kFirstUpdate;
        return true;
    }
    if ((flags & kEndMask) != 0) return false;
    if ((flags & kStageTransition) != 0) {
        ++transition_frame;
        if (transition_frame >= transition_length) {
            flags &= ~kStageTransition;
            transition_frame = 0;
        }
    }
    ++globals.game_frame;
    ++globals.stage_frame;
    timer.tick(game_speed);
    return true;
}

}  // namespace th16
