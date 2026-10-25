// HUD 单测：残机/灵击句柄的 interrupt 布局（2=满/3=空/7+n=碎片，dump
// front.anm script30/38 已验证）、分数逼近与文本格式。
#include "ui_test_fixtures.hpp"
#include "../harness/test_framework.hpp"

#include "game/GameHud.hpp"
#include "game/HudDrawData.hpp"
#include "game/FrameScheduler.hpp"

#include <string>
#include <vector>

using namespace th13;
using namespace th13::test;

namespace {
struct HudHarness {
    GameRandom random;
    AnmEnvironment environment;
    AnmManager manager{random, environment};
    SessionState progress;
    HudPlayerState player;
    HudScoreState score;
    FrameScheduler scheduler;
    GameHud hud{manager, progress, player, score, 3, 2};
    struct SoundStub : HudFrameServices {
        bool hud_sound(i32) override { return true; }
    } sound;
    struct TextSink : HudDrawServices {
        std::vector<std::string> lines;
        bool hud_text(const HudTextDraw& draw) override {
            lines.push_back(draw.text);
            return true;
        }
    } sink;
    HudHarness() {
        // front bank：root(0) + 残机 30–37 + 灵击 38–45 + 难度 79/85 + 灵界计
        // 171（GameHud.cpp 生产语义要求夹具自带 171，不吞错）。
        auto bank = synthetic_anm(8, trivial_scripts(172));
        CHECK(load_bank(manager, 3, bank));
        // ascii bank：boss 计时数字 VM（script2/3，0x425170:181-287 池模板）。
        CHECK(load_bank(manager, 2, synthetic_anm(302, trivial_scripts(46))));
        CHECK(hud.initialize(HudEntryContext{}));
    }
};
i32 pending(const AnmManager& manager, u32 handle) {
    auto* vm = const_cast<AnmManager&>(manager).registry.find(handle);
    return vm ? vm->pending_interrupt : -999;
}
}  // namespace

TEST(life_icons_interrupt_layout) {
    HudHarness harness;
    // 3 残机 + 2 碎片：图标 0-2 满(2)、图标 3 碎片档、图标 4-7 空(3)。
    // 档位 = (pieces*5)/threshold+7（0x42ae90:34-40；threshold=0x4bb994[be7fc]
    // 首项 8，SessionState.hpp 表实证）：(2*5)/8+7 = 8。
    CHECK(harness.hud.life(3, 2));
    for (int i = 0; i < 3; i++) CHECK_EQ(pending(harness.manager, harness.hud.life_icons[i]), 2);
    CHECK_EQ(pending(harness.manager, harness.hud.life_icons[3]), 8);
    for (int i = 4; i < 8; i++) CHECK_EQ(pending(harness.manager, harness.hud.life_icons[i]), 3);
    CHECK(harness.hud.life(8, 0));  // 满 8：全满，无碎片位
    for (int i = 0; i < 8; i++) CHECK_EQ(pending(harness.manager, harness.hud.life_icons[i]), 2);
    CHECK(!harness.hud.life(9, 0));  // 越界显式失败
}

TEST(bomb_icons_interrupt_layout) {
    HudHarness harness;
    CHECK(harness.hud.bombs(2, 4));
    for (int i = 0; i < 2; i++) CHECK_EQ(pending(harness.manager, harness.hud.bomb_icons[i]), 2);
    // 0x42af60:38：(4*6)/8+7 = 10（灵击分母固定 8）。
    CHECK_EQ(pending(harness.manager, harness.hud.bomb_icons[2]), 10);
    for (int i = 3; i < 8; i++) CHECK_EQ(pending(harness.manager, harness.hud.bomb_icons[i]), 3);
    CHECK(!harness.hud.bombs(-1, 0));
}

TEST(score_display_converges_and_records_high) {
    HudHarness harness;
    harness.score.score = 130000;
    harness.progress.high_score = 0;
    for (int frame = 0; frame < 300 && harness.hud.displayed_score < harness.score.score; frame++) {
        harness.hud.update_score();
        CHECK(harness.hud.displayed_score <= harness.score.score);  // 不超调
    }
    CHECK_EQ(harness.hud.displayed_score, 130000);
    CHECK_EQ(harness.progress.high_score, 130000);  // hi-score 同步
}

TEST(score_and_grouped_number_formats) {
    CHECK_EQ(grouped_number(0), std::string("0"));
    CHECK_EQ(grouped_number(999), std::string("999"));
    CHECK_EQ(grouped_number(1000), std::string("1,000"));
    CHECK_EQ(grouped_number(12345678), std::string("12,345,678"));
    CHECK_EQ(score_number(0, 0), std::string("0"));
    CHECK_EQ(score_number(12, 3), std::string("123"));
    CHECK_EQ(score_number(123, 4), std::string("1,234"));
    CHECK_EQ(score_number(12345, 6), std::string("123,456"));
    CHECK_EQ(score_number(123456789, 7), std::string("1,234,567,897"));
}

TEST(hud_text_draw_pass_paints_power_and_pieces) {
    HudHarness harness;
    harness.player.power = 342;  // 3.42 / 4.00
    harness.hud.displayed_score = 100;
    CHECK(harness.hud.draw(HudDrawContext{}, harness.sink));
    CHECK(harness.sink.lines.size() >= 8);
    bool has_power_integer = false, has_power_fraction = false, has_pieces = false;
    for (const auto& line : harness.sink.lines) {
        if (line == "3.") has_power_integer = true;
        if (line == "42") has_power_fraction = true;
        // 碎片分母 = 阈值表首项 8（0x4bb994 .data 实证，SessionState.hpp）。
        if (line == "/8") has_pieces = true;
    }
    CHECK(has_power_integer);
    CHECK(has_power_fraction);
    CHECK(has_pieces);  // 残机/灵击碎片分母
}

int main() { return th13test::run_all("游戏 HUD"); }
