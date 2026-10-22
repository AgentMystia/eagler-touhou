// 对局内覆盖层 PostGameFlow 单测（S3 第一阶段）：0x42cd40 关终了主干
//（每关百万奖/换关请求/关号推进）、计分公式边界（点值制与 999,999,999 封顶）、
// 六面通关演出 180f/379f、Extra/练习/符卡练习/全灭入口分流。
#include "../harness/test_framework.hpp"

#include "game/PostGameFlow.hpp"
#include "game/ScoreBoard.hpp"

#include <vector>

using namespace th13;

namespace {

struct RecordPlatform final : PostGamePlatform {
    std::vector<i32> requests, bgms;
    void request_destination(i32 raw) override { requests.push_back(raw); }
    void bgm(i32 id) override { bgms.push_back(id); }
};

struct FlowHarness {
    SessionState progress;
    HudPlayerState player;
    HudScoreState score;
    RecordPlatform host;
    PostGameFlow flow{progress, player, score, host};

    FlowHarness() {
        progress.stage = 1;
        progress.difficulty = 1;  // Normal
        player.extra_lives = 2;
        player.bombs = 2;
    }
};

}  // namespace

TEST(score_formulas_byte_semantics) {
    // 0x42abf0：bonus = 关号×1,000,000（点值 /10 入账）。
    CHECK_EQ(stage_clear_bonus(1), 1000000);
    CHECK_EQ(stage_clear_bonus(6), 6000000);
    // 0x42ce96-0x42ced0 四难度六面全通奖（残=3 灵=1 例）。
    CHECK_EQ(all_clear_bonus(0, 3, 1), (3 + 1 * 4) * 250000);
    CHECK_EQ(all_clear_bonus(1, 3, 1), (3 + 1 * 3) * 1000000);
    CHECK_EQ(all_clear_bonus(2, 3, 1), 1 * 5000000 + 3 * 1500000);
    CHECK_EQ(all_clear_bonus(3, 3, 1), (3 + 1 * 5) * 2000000);
    // 0x42cfb3：Extra (残+灵×10)×4,000,000。
    CHECK_EQ(extra_clear_bonus(2, 3), (2 + 3 * 10) * 4000000);
    // 点值制入账 + 封顶（0x3b9ac9ff，0x42cef3/0x42cfe7 比较语义）。
    CHECK_EQ(add_bonus_capped(0, 1000000), 100000u);
    CHECK_EQ(add_bonus_capped(999999999, 1000000), 999999999u);
    CHECK_EQ(add_bonus_capped(999999900, 1000000), 999999999u);  // +100000 → 超 → 钳
    CHECK_EQ(add_bonus_capped(500000, -1000000), 400000u);
}

TEST(stage1_clear_requests_next_stage) {
    FlowHarness harness;
    harness.flow.on_stage_clear();
    CHECK_EQ(harness.host.requests.size(), 1u);
    CHECK_EQ(harness.host.requests[0], 0xc);  // dcc24=0xc 无条件（0x42cd40:103）
    CHECK_EQ(harness.progress.stage, 2);      // be81c++（<7）
    CHECK_EQ(harness.score.score, 100000);    // 1,000,000/10 点值
    CHECK_EQ(harness.progress.stage_clear_bonus, 1000000);
    CHECK(harness.flow.state() == OverlayState::Monitor);  // 换关非覆盖层
    CHECK(!harness.flow.frozen());
    CHECK_EQ(harness.progress.stage_cleared_bits, 1u << (1 + 1 * 8));
}

TEST(stage5_clear_caps_score) {
    FlowHarness harness;
    harness.progress.stage = 5;
    harness.score.score = 999999999 - 50000;  // 5M/10=500000 入账必超顶
    harness.flow.on_stage_clear();
    CHECK_EQ(harness.score.score, 999999999);
    CHECK_EQ(harness.host.requests.size(), 1u);
    CHECK_EQ(harness.progress.stage, 6);
}

TEST(practice_clear_stage_chain_and_gates) {
    // 0x42cd40 else 支（关 1-5）：dcc24=0xc 无条件——练习也换关；练习门只拦
    // score 块的 0x7fc 关卡通过位（0x42cd40:97-99）。
    FlowHarness harness;
    harness.progress.practice_mode = 1;  // GameInf+0x74
    harness.progress.stage = 3;
    harness.flow.on_stage_clear();
    CHECK_EQ(harness.host.requests.size(), 1u);
    CHECK_EQ(harness.host.requests[0], 0xc);
    CHECK_EQ(harness.progress.stage, 4);
    CHECK_EQ(harness.score.score, 300000);     // 0x42abf0 只被符卡练习门拦
    CHECK_EQ(harness.progress.stage_cleared_bits, 0u);  // 练习不记通过位
    CHECK(!harness.flow.frozen());             // 换关路径不进覆盖层
}

TEST(spell_practice_skips_stage_bonus) {
    FlowHarness harness;
    harness.progress.run_mode_flags = 0x20;  // be830&0x30 == 0x20（mode2）
    harness.flow.on_stage_clear();
    CHECK_EQ(harness.score.score, 0);  // 0x42abf0 被 &0x30==0x20 门跳过
    CHECK(harness.flow.state() == OverlayState::BSpellVariant);  // 0x43f720→0x1e
    CHECK(harness.flow.frozen());
}

TEST(stage6_all_clear_and_ending_sequence) {
    FlowHarness harness;
    harness.progress.stage = 6;
    harness.progress.difficulty = 1;  // Normal：(残+灵×3)×1,000,000
    harness.progress.ending_points = 600;  // >599 → 好结局资格 +1
    harness.flow.on_stage_clear();
    // 6M（每关奖）+ (2+2*3)*1M=8M（六面全通）= 14M → 点值 1,400,000。
    CHECK_EQ(harness.score.score, 1400000);
    CHECK_EQ(harness.progress.stage_clear_bonus, 14000000);
    CHECK_EQ(harness.progress.good_endings, 1);
    CHECK(harness.host.requests.empty());      // 0x4000 演出期间无请求
    CHECK(harness.flow.clear_sequence());
    // 180f 淡出 / >379f 出口（0x42c550:20-57）；覆盖层冻结会挂起演出。
    for (u32 frame = 0; frame < 379; frame++) harness.flow.update();
    CHECK(harness.host.requests.empty());
    harness.flow.update();  // 第 380 帧
    CHECK_EQ(harness.host.requests.size(), 1u);
    CHECK_EQ(harness.host.requests[0], 0xf);  // 请求结局
    CHECK(!harness.flow.clear_sequence());
    // 残>=3 时资格计数置 3（0x42cd40:36-38）。
    FlowHarness capped;
    capped.progress.stage = 6;
    capped.player.extra_lives = 3;
    capped.flow.on_stage_clear();
    CHECK_EQ(capped.progress.good_endings, 3);
}

TEST(stage6_practice_ending_exits_to_settlement_a) {
    FlowHarness harness;
    harness.progress.stage = 6;
    harness.progress.practice_mode = 1;
    harness.flow.on_stage_clear();
    CHECK(harness.flow.clear_sequence());  // 练习六面同样走 0x4000 演出
    for (u32 frame = 0; frame < 380; frame++) harness.flow.update();
    CHECK(harness.flow.state() == OverlayState::PracticeEntry);  // 0x43e5a0
    CHECK(harness.host.requests.empty());
}

TEST(stage7_extra_clear_bonus) {
    FlowHarness harness;
    harness.progress.stage = 7;  // be81c=7（Extra 关号；be830 保持 mode0）
    harness.flow.on_stage_clear();
    // 7M（每关奖）+ (2+2*10)*4M=88M（Extra 奖）= 95M → 点值 9,500,000。
    CHECK_EQ(harness.score.score, 9500000);
    CHECK(harness.flow.state() == OverlayState::BShow);  // 0x43f720 mode0→0x17
    CHECK(harness.flow.frozen());
    CHECK(harness.host.requests.empty());
}

TEST(game_over_entry_freezes) {
    FlowHarness harness;
    harness.flow.on_game_over();  // 0x43f570（mode0 → 态 0xe）
    CHECK(harness.flow.state() == OverlayState::GameOverShow);
    CHECK(harness.flow.frozen());
    CHECK_EQ(harness.host.bgms.size(), 1u);  // BGM th128_08（音频 TODO 记号）
    CHECK(harness.host.requests.empty());
}

int main() { return th13test::run_all("对局内覆盖层"); }
