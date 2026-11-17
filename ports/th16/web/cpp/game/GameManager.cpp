#include "GameManager.hpp"

#include "AnmManager.hpp"
#include "GameGlobals.hpp"

namespace th16 {

void GameManager::first_frame(AnmManager& manager, GameGlobals& globals) {
    // 0x42dc50：清重试位；0x40add0 建面 VM、启用关卡链；帧计数清零；0x41aa70 生成 "main"
    flags &= ~kRetry;
    if (stage_live) stage.start(manager);
    if (player_live) player.reset(manager, globals);   // 0x441740
    globals.game_frame = 0;
    globals.stage_frame = 0;
    if (enemies_live) {
        if (!enemies.spawn_main()) {
            // 生成失败时置装载失败位；上层会在 failure 中看到 enemies.error
            flags |= kLoadFailed;
        }
        // enemy_count 由敌人更新链同步；harness 桩可覆盖，避免此处抢写
    }
}

bool GameManager::update(AnmManager& manager, GameGlobals& globals, float game_speed) {
    // 0x42d7b0
    if ((flags & kStageClear) != 0) {
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
