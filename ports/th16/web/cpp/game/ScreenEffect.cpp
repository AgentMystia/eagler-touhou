// th16 ScreenEffect kind 1——原版 0x45cd30。
#include "ScreenEffect.hpp"

namespace th16 {

bool screen_shake_step(ScreenShake& shake, Rng& game_rng, float window_scale, float game_speed,
                       ScreenShakeOffset& out) {
    shake.timer.speed_slot = 0;
    shake.timer.tick(game_speed);
    if (shake.duration <= shake.timer.current) return false;
    const float amp = (float(shake.amp_to - shake.amp_from) * shake.timer.current_f) / float(shake.duration) +
                      float(shake.amp_from);
    switch (game_rng.next32() % 3u) {
    case 0:
        out.offset.x = 0.0f;
        out.offset_scaled.x = 0.0f;
        break;
    case 1:
        out.offset_scaled.x = window_scale * amp;
        out.offset.x = amp;
        break;
    default:
        out.offset_scaled.x = window_scale * amp;
        out.offset.x = -amp;
        break;
    }
    switch (game_rng.next32() % 3u) {
    case 0:
        out.offset.y = 0.0f;
        out.offset_scaled.y = 0.0f;
        break;
    case 1:
        out.offset.y = amp;
        out.offset_scaled.y = window_scale * amp;
        break;
    default:
        out.offset.y = -amp;
        out.offset_scaled.y = -(window_scale * amp);
        break;
    }
    return true;
}

}  // namespace th16
