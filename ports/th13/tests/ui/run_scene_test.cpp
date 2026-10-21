// 对局场景单测：HUD 句柄建立、帧推进/分数逼近、暂停冻结、Esc/失焦请求、
// Stage1 战斗装载（合成 ECL/ANM）与操控步进。
#include "ui_test_fixtures.hpp"
#include "../ecl/ecl_test_fixtures.hpp"
#include "../harness/test_framework.hpp"

#include "game/RunScene.hpp"
#include "game/FrameScheduler.hpp"

#include <map>

using namespace th13;
using namespace th13::test;

namespace {
// 合成 stage ECL：main = wait(600) 后 return（root 结束 → 敌链空转，见
// StageSim::step_frame 空链门）；ANIM 名单两项供装载核验。
std::vector<u8> synthetic_stage_ecl() {
    std::vector<u8> main;
    th13test::emit(main, 0, 23, 0, 0xff, 0, {600});  // wait
    th13test::emit(main, 0, 10, 0, 0xff, 0, {});     // return
    return th13test::build_scpt({{"main", main}}, {"stage_a.anm", "stage_b.anm"});
}

struct RunHost final : RunSceneServices {
    AnmManager& manager;
    int pauses = 0, game_overs = 0;
    std::vector<std::string> lines;
    std::map<std::string, std::vector<u8>> assets;
    explicit RunHost(AnmManager& manager) : manager(manager) {
        assets["st01.ecl"] = synthetic_stage_ecl();
    }
    bool request_pause() override {
        ++pauses;
        return true;
    }
    bool request_game_over() override {
        ++game_overs;
        return true;
    }
    bool hud_sound(i32) override { return true; }
    bool life_hud(i32, i32) override { return true; }
    bool bomb_hud(i32, i32) override { return true; }
    bool camera(DrawCamera, bool) override { return true; }
    bool hud_text(const HudTextDraw& draw) override {
        lines.push_back(draw.text);
        return true;
    }
    bool read_asset(const std::string& name, std::vector<u8>& bytes) override {
        const auto found = assets.find(name);
        if (found == assets.end()) return false;
        bytes = found->second;
        return true;
    }
};

// ZunGraphics 纯虚桩：对局场景单测只驱动逻辑帧与文本 pass，不落 GPU。
struct NullGraphics final : ZunGraphics {
    touhou::graphics::PipelineState& pipeline() override { return pipeline_state; }
    u32 texture(const AnmResource&, u32) override { return 0; }
    void bind_texture(u32) override {}
    void set_layout(touhou::graphics::VertexLayout) override {}
    void set_matrix(touhou::graphics::MatrixKind, const Matrix4&) override {}
    void triangles(u32, const void*, u32) override {}
    void primitives(touhou::graphics::Topology, u32, const void*, u32) override {}
    bool select_target(const AnmResource*, u32) override { return true; }
    bool clear_target(u32, const GraphicsViewport*) override { return true; }
    bool clear_depth(const GraphicsViewport*) override { return true; }
    void set_viewport(const GraphicsViewport&) override {}

private:
    touhou::graphics::PipelineState pipeline_state;
};

struct RunHarness {
    GameRandom random;
    AnmEnvironment environment;
    AnmManager manager{random, environment};
    NullGraphics graphics;
    AnmRenderer renderer{graphics};
    SessionState progress;
    HudPlayerState player;
    HudScoreState score;
    FrameScheduler scheduler;
    RunHost host{manager};
    std::unique_ptr<RunScene> scene;

    RunHarness() {
        CHECK(load_bank(manager, 3, synthetic_anm(8, trivial_scripts(46))));  // front 0..45
        CHECK(load_bank(manager, 4, synthetic_anm(8, trivial_scripts(4))));   // stage 合并位
        CHECK(load_bank(manager, 5, synthetic_anm(8, trivial_scripts(1))));   // bullet
        CHECK(load_bank(manager, 6, synthetic_anm(64, trivial_scripts(3))));  // player 0..2
        host.assets["stage_a.anm"] = synthetic_anm(8, trivial_scripts(1));
        host.assets["stage_b.anm"] = synthetic_anm(8, trivial_scripts(1));
        StagePresentation::Banks banks{4, 5, 6};
        scene = std::make_unique<RunScene>(progress, player, score, manager, renderer,
                                           environment, host, scheduler, banks, 3, 2);
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

TEST(run_scene_loads_stage_battle) {
    RunHarness harness;
    // ANIM 名单核验（ECL 头声明 → provider 读名）。
    CHECK_EQ(harness.scene->battle.sim.anim_list.size(), 2u);
    CHECK_EQ(harness.scene->battle.sim.anim_list[0], std::string("stage_a.anm"));
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

TEST(run_scene_player_controls_and_shots) {
    RunHarness harness;
    // Z 按住 + 左移 60 帧：自机左移、自机弹生成。
    harness.scene->controls(0x40 | 0x01, 0x01, 0x01);
    const float start_x = harness.scene->battle.player_x;
    for (int frame = 0; frame < 60; frame++) CHECK(harness.scheduler.update() >= 0);
    CHECK(harness.scene->battle.player_x < start_x - 100.0f);
    CHECK(!harness.scene->battle.shots.empty());
    CHECK(harness.scene->error.empty());
}

TEST(run_scene_bomb_consumes_stock_and_clears) {
    RunHarness harness;
    const i32 bombs = harness.player.bombs;
    harness.scene->controls(0x02, 0x02, 0x02);  // X 边沿
    CHECK(harness.scheduler.update() >= 0);
    CHECK_EQ(harness.player.bombs, bombs - 1);
    CHECK(harness.scene->battle.bomb_timer > 0);
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
