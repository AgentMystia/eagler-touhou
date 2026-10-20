// 标题状态机单测：主菜单 → 难度 → 角色 → 开始游戏 全链路（合成 ANM bank），
// 以及 Quit 出口（destination 语义见 ui-base.md §四）。
#include "ui_test_fixtures.hpp"
#include "../harness/test_framework.hpp"

#include "game/TitleScene.hpp"
#include "game/FrameScheduler.hpp"

using namespace th13;
using namespace th13::test;

namespace {
struct TitleHost final : TitleScenePlatform {
    AnmManager& manager;
    i32 start_stage = -1, exit_destination = -1;
    int sounds = 0;
    explicit TitleHost(AnmManager& manager) : manager(manager) {}
    bool clear_title_overlay() override { return true; }
    bool queue_title_music(i32, i32, const std::string&) override { return true; }
    bool clear_current_wave() override { return true; }
    bool fade_out_title_music() override { return true; }
    bool sound(i32) override {
        ++sounds;
        return true;
    }
    bool begin_transition(u32& animation) override {
        animation = manager.create(8, 0, -1, 0);
        return animation != 0;
    }
    bool start_game(i32 stage) override {
        if (start_stage >= 0) return false;  // 只应请求一次
        start_stage = stage;
        return true;
    }
    bool hud_text(const HudTextDraw&) override { return true; }
    // StartGame 与 Quit 共享 TitleScreen 值：开局请求排队后不覆盖（原版守卫）。
    bool title_exit(i32 destination) override {
        if (start_stage >= 0) return true;
        exit_destination = destination;
        return true;
    }
};

struct TitleHarness {
    GameRandom random;
    AnmEnvironment environment;
    AnmManager manager{random, environment};
    SessionState progress;
    TitleSelectionSettings selection;
    FrameScheduler scheduler;
    TitleHost host{manager};
    std::unique_ptr<TitleScene> scene;

    TitleHarness() {
        // title bank：script0 在创建时派生菜单子脚本 3..22；其余平凡。
        std::vector<std::vector<AnmIns>> title_scripts = trivial_scripts(24);
        title_scripts[0].clear();
        for (int child = 3; child <= 22; child++) title_scripts[0].push_back(spawn(child));
        title_scripts[0].push_back(wait());
        title_scripts[0].push_back(stop());
        CHECK(load_bank(manager, 16, synthetic_anm(8, title_scripts)));
        CHECK(load_bank(manager, 17, synthetic_anm(1, trivial_scripts(1))));
        CHECK(load_bank(manager, 2, synthetic_anm(8, trivial_scripts(24))));  // ascii（prompt=19）
        CHECK(load_bank(manager, 8, synthetic_anm(4, trivial_scripts(4))));   // effect 转场
        scene = std::make_unique<TitleScene>(progress, selection, manager, host, scheduler,
                                             TitleSceneResources{});
        CHECK(scene->error.empty());
    }

    void frame(u32 pressed = 0, u32 repeated = 0) {
        scene->controls({0, pressed, repeated, 0});
        CHECK(scheduler.update() >= 0);
    }

    void advance(int frames, u32 pressed = 0, u32 repeated = 0) {
        for (int i = 0; i < frames; i++) frame(pressed, repeated);
    }
};
}  // namespace

TEST(title_full_chain_start_game) {
    TitleHarness harness;
    // 入场：Initialize → Main，等待提示幕与主菜单展开（age 120 → >130）。
    harness.advance(1);
    CHECK_EQ(int(harness.scene->state.screen), int(TitleScreen::Main));
    harness.advance(20);  // 越过 age==120 建根与 >130 展开闸
    CHECK_EQ(harness.scene->state.substate, 2);
    // 确认（Z）：StartGame → 难度幕。
    harness.frame(1);
    harness.advance(25);  // substate 4 的 age>19 转场
    CHECK_EQ(int(harness.scene->state.screen), int(TitleScreen::Difficulty));
    // 难度：默认 Normal(1)；右移到 Hard(2) 后确认。
    harness.advance(10);  // substate 0→1→2
    CHECK_EQ(harness.scene->state.substate, 2);
    harness.frame(0xa0);  // →|↓ 掩码：下一项
    CHECK_EQ(harness.scene->state.menu.cursor, 2);
    harness.frame(1);  // 确认 Hard
    harness.advance(20);
    CHECK_EQ(int(harness.scene->state.screen), int(TitleScreen::Character));
    CHECK_EQ(harness.progress.difficulty, 2);
    // 角色：右移到 Marisa(1)，左移回 Reimu(0)，再右移到 Marisa 后确认。
    harness.advance(10);
    CHECK_EQ(harness.scene->state.substate, 2);
    harness.frame(0x80);
    CHECK_EQ(harness.scene->state.menu.cursor, 1);
    harness.frame(0x40);
    CHECK_EQ(harness.scene->state.menu.cursor, 0);
    harness.frame(0x80);
    CHECK_EQ(harness.scene->state.menu.cursor, 1);
    harness.frame(1);  // 确认：转场 + age>39 → start_game
    harness.advance(45);
    CHECK_EQ(harness.host.start_stage, 1);
    CHECK_EQ(harness.progress.character, 1);
    CHECK_EQ(int(harness.scene->state.screen), int(TitleScreen::StartGame));
    CHECK(harness.scene->error.empty());
    CHECK_EQ(harness.host.exit_destination, -1);  // 开局排队后 Quit 分支不触发
}

TEST(title_quit_requests_exit_destination) {
    TitleHarness harness;
    harness.advance(25);
    CHECK_EQ(harness.scene->state.substate, 2);
    // Replay/PlayerData/MusicRoom 禁用：一次下移直接落到 Quit(4)。
    harness.frame(0x20);
    CHECK_EQ(harness.scene->state.menu.cursor, 4);
    harness.frame(1);
    harness.advance(25);
    CHECK_EQ(int(harness.scene->state.screen), int(TitleScreen::Quit));
    harness.advance(2);  // TitleFrame case 2：title_exit
    CHECK_EQ(harness.host.exit_destination, 3);  // 原版语义：退出
    CHECK(harness.scene->error.empty());
}

TEST(title_disabled_entries_not_selectable) {
    TitleHarness harness;
    harness.advance(25);
    for (int i = 0; i < 6; i++) harness.frame(0x20);  // 充分环绕
    const i32 cursor = harness.scene->state.menu.cursor;
    CHECK(cursor == 0 || cursor == 4);  // 只停在 StartGame/Quit
}

int main() { return th13test::run_all("标题状态机"); }
