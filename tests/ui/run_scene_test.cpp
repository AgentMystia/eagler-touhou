// 对局场景单测：HUD 句柄建立、帧推进/分数逼近、暂停冻结、Esc/失焦请求。
#include "ui_test_fixtures.hpp"
#include "../harness/test_framework.hpp"

#include "game/RunScene.hpp"
#include "game/FrameScheduler.hpp"

using namespace th13;
using namespace th13::test;

namespace {
struct RunHost final : RunSceneServices {
    AnmManager& manager;
    int pauses = 0, game_overs = 0;
    std::vector<std::string> lines;
    explicit RunHost(AnmManager& manager) : manager(manager) {}
    bool request_pause() override {
        ++pauses;
        return true;
    }
    bool request_game_over() override {
        ++game_overs;
        return true;
    }
    bool hud_sound(i32) override { return true; }
    bool hud_text(const HudTextDraw& draw) override {
        lines.push_back(draw.text);
        return true;
    }
};

struct RunHarness {
    GameRandom random;
    AnmEnvironment environment;
    AnmManager manager{random, environment};
    SessionState progress;
    HudPlayerState player;
    HudScoreState score;
    FrameScheduler scheduler;
    RunHost host{manager};
    std::unique_ptr<RunScene> scene;

    RunHarness() {
        CHECK(load_bank(manager, 3, synthetic_anm(8, trivial_scripts(46))));  // front 0..45
        scene = std::make_unique<RunScene>(progress, player, score, manager, environment, host,
                                           scheduler, 3, 2);
        CHECK(scene->initialize());
    }
};
}  // namespace

TEST(run_scene_initializes_hud_handles) {
    RunHarness harness;
    CHECK(harness.scene->hud.root != 0);
    for (u32 i = 0; i < 8; i++) {
        CHECK(harness.scene->hud.life_icons[i] != 0);
        CHECK(harness.scene->hud.bomb_icons[i] != 0);
    }
    CHECK(harness.scene->error.empty());
}

TEST(run_scene_frame_and_score_convergence) {
    RunHarness harness;
    harness.score.score = 40000;
    const i32 before = harness.progress.stage_frame;
    for (int frame = 0; frame < 200; frame++) CHECK(harness.scheduler.update() >= 0);
    CHECK(harness.progress.stage_frame > before + 100);   // 帧钟推进
    CHECK_EQ(harness.scene->hud.displayed_score, 40000);  // 分数收敛
    CHECK(harness.scene->error.empty());
}

TEST(run_scene_draws_hud_text) {
    RunHarness harness;
    harness.scene->hud.displayed_score = 0;
    CHECK(harness.scheduler.update() >= 0);  // 更新 pass
    // 文本 pass 由 Draw 回调驱动：直接调用一次绘制入口收集字符串。
    harness.host.lines.clear();
    CHECK(harness.scene->draw());
    CHECK(harness.host.lines.size() >= 6);  // hi-score/score/碎片/灵力族
    CHECK(harness.scene->error.empty());
}

TEST(run_scene_freezes_when_paused) {
    RunHarness harness;
    for (int i = 0; i < 10; i++) CHECK(harness.scheduler.update() >= 0);
    const i32 frozen = harness.progress.stage_frame;
    harness.environment.paused = true;  // 暂停覆盖层置位（ApplicationState 同机制）
    for (int i = 0; i < 10; i++) CHECK(harness.scheduler.update() >= 0);
    CHECK_EQ(harness.progress.stage_frame, frozen);
    harness.environment.paused = false;
    CHECK(harness.scheduler.update() >= 0);
    CHECK(harness.progress.stage_frame > frozen);
}

TEST(run_scene_requests_pause_and_game_over) {
    RunHarness harness;
    harness.scene->request_pause();
    harness.scene->request_game_over();
    CHECK_EQ(harness.host.pauses, 1);
    CHECK_EQ(harness.host.game_overs, 1);
}

int main() { return th13test::run_all("对局场景"); }
