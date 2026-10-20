// 游戏 HUD（th15 GameHud 裁剪：残机/灵击/分数/灵力保留；boss 血环、符卡计时、
// pointdevice、chapter 通知延后；trance 槽仅留成员占位，docs/plans/ui-base.md §五）。
// HUD 只持动画句柄；文本/数字绘制是独立表现 pass，不持设备对象。
#pragma once

#include "AnmManager.hpp"
#include "SessionState.hpp"
#include "HudFrameData.hpp"
#include "HudDrawData.hpp"

namespace th13 {

struct HudEntryContext {
    i32 scene_destination = 0;
};

class GameHud {
    AnmManager& animations;
    SessionState& progress;
    HudPlayerState& player;
    HudScoreState& score;
    i32 front, ascii;

    bool check(bool);
    u32 create(i32 bank, i32 script, u32 ordering = 0);
    u32 widget(i32 script);
    bool direct_interrupt(u32, i32);

public:
    std::array<u32, 8> life_icons{}, bomb_icons{};
    u32 root = 0, difficulty_notice = 0, difficulty_label = 0;
    u32 flags = 0;
    Timer intro_age{0, 0, 0, 0, 0};
    Timer frame_age{0, 0, 0, 0, 0};
    i32 displayed_score = 0, score_increment = 0;
    std::string error;
    // th13 特有（本轮不做，仅留占位）：灵界槽，见 docs/plans/ui-base.md §五。
    u32 trance_gauge = 0, trance_frame = 0;

    GameHud(AnmManager& a, SessionState& p, HudPlayerState& s, HudScoreState& v, i32 front_bank,
            i32 ascii_bank)
        : animations(a), progress(p), player(s), score(v), front(front_bank), ascii(ascii_bank) {
        frame_age.set(0);
    }

    bool initialize(const HudEntryContext&);
    bool life(i32 lives, i32 pieces);
    bool bombs(i32 bombs, i32 pieces);
    bool stage_logo(i32 scene_destination);
    bool update(const HudFrameContext&, HudFrameServices&);
    void update_score();
    bool draw(const HudDrawContext&, HudDrawServices&);
    bool reset_for_retry();
    void prepare_stage_assets() noexcept {
        frame_age.set(0);
        displayed_score = score.score;
    }
    void update_trance() noexcept {}  // 神灵/灵界分期（AddressMap.hpp spirit 条目）
};

}  // namespace th13
