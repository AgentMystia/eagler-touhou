// GameHud 逻辑帧 pass（th15 裁剪：boss/符卡/通知驱动延后，仅保帧钟与文本前提）。
#include "GameHud.hpp"

namespace th13 {

bool GameHud::update(const HudFrameContext& context, HudFrameServices& services) {
    animations.rate = progress.rate;
    const auto sound = [&](i32 id) {
        if (services.hud_sound(id)) return true;
        error = "HUD sound service failed";
        return false;
    };
    (void)sound;  // 通知/符卡音效随对应分期接入
    if (context.player_position) {
        // th15 0x200 标志（玩家贴上沿隐藏 boss 部件）随 boss HUD 分期。
    }
    frame_age.tick(&progress.rate);
    return true;
}

}  // namespace th13
