// 暂停覆盖层单测：Esc 打开、菜单三项（继续/重开/回标题）、Game Over 禁用继续、
// 失焦与恢复（ui-base.md §四 Run 分支）。
#include "ui_test_fixtures.hpp"
#include "../harness/test_framework.hpp"

#include "game/RunPause.hpp"
#include "game/FrameScheduler.hpp"

using namespace th13;
using namespace th13::test;

namespace {
struct PauseHost final : RunPausePlatform {
    bool sound(i32) override { return true; }
    bool suspend_music() override { return true; }
    int restarts = 0, titles = 0;
    bool restart_run() override {
        ++restarts;
        return true;
    }
    bool return_to_title() override {
        ++titles;
        return true;
    }
};

struct PauseHarness {
    GameRandom random;
    AnmEnvironment environment;
    AnmManager manager{random, environment};
    SessionState progress;
    FrameScheduler scheduler;
    PauseHost host;
    std::unique_ptr<RunPause> pause;

    PauseHarness() : host() {
        // 数据面与生产对齐（RunPause 16:05 轮原版锚定后 fixture 未随更，属
        // fixture 不足）：背景 s75 出自常驻 text.anm（bank 0，DAT_004dcc54，
        // kPauseBackdropScript=75）；front.anm（bank 3）根 132/133/142、
        // 行条 120-123/154、光标 130/131（0043e504/0043e4f0/0043fb7d 族）。
        // 断言原样未动。
        CHECK(load_bank(manager, 0, synthetic_anm(8, trivial_scripts(76))));   // text
        CHECK(load_bank(manager, 3, synthetic_anm(8, trivial_scripts(155))));  // front
        progress.rate = 1;
        pause = std::make_unique<RunPause>(progress, manager, environment, host, scheduler, 3);
        CHECK(pause->prepare());
    }

    void frame(u32 pressed = 0, bool lost_focus = false) {
        pause->controls({pressed, 0, lost_focus});
        CHECK(scheduler.update() >= 0);
    }

    void advance(int frames, u32 pressed = 0) {
        for (int i = 0; i < frames; i++) frame(pressed);
    }
};
}  // namespace

TEST(pause_open_and_continue) {
    PauseHarness harness;
    CHECK(!harness.pause->active());
    CHECK(harness.pause->open(PauseEntrance::Pause));
    CHECK(harness.pause->active());
    CHECK_EQ(harness.progress.scene_flags & 0x10u, 0x10u);
    harness.advance(12);  // 入场延迟 age>9 → 菜单展开
    CHECK_EQ(harness.pause->state.phase, 6);
    CHECK_EQ(harness.pause->state.menu.cursor, 0);
    CHECK_EQ(harness.pause->state.menu.count, 3);
    harness.frame(1);  // 确认「继续」
    harness.advance(15);
    CHECK(!harness.pause->active());
    CHECK_EQ(harness.host.restarts, 0);
    CHECK_EQ(harness.host.titles, 0);
    CHECK_EQ(harness.progress.scene_flags & 0x10u, 0u);  // 旗标复位
    CHECK(harness.pause->error.empty());
}

TEST(pause_esc_resumes_like_continue) {
    PauseHarness harness;
    CHECK(harness.pause->open(PauseEntrance::Pause));
    harness.advance(12);
    harness.frame(0x100);  // Esc
    harness.advance(15);
    CHECK(!harness.pause->active());
    CHECK_EQ(harness.host.restarts, 0);
}

TEST(pause_cursor_navigates_three_entries) {
    PauseHarness harness;
    CHECK(harness.pause->open(PauseEntrance::Pause));
    harness.advance(12);
    harness.frame(0x20);  // ↓
    CHECK_EQ(harness.pause->state.menu.cursor, 1);
    harness.frame(0x20);
    CHECK_EQ(harness.pause->state.menu.cursor, 2);
    harness.frame(0x10);  // ↑
    CHECK_EQ(harness.pause->state.menu.cursor, 1);
}

TEST(pause_restart_and_title_destinations) {
    PauseHarness harness;
    CHECK(harness.pause->open(PauseEntrance::Pause));
    harness.advance(12);
    harness.frame(0x20);  // ↓ → 重开(1)
    harness.frame(1);
    harness.advance(15);
    CHECK(!harness.pause->active());
    CHECK_EQ(harness.host.restarts, 1);

    CHECK(harness.pause->open(PauseEntrance::Pause));  // 再次暂停
    harness.advance(12);
    harness.frame(0x20);
    harness.frame(0x20);  // ↓↓ → 回标题(2)
    harness.frame(1);
    harness.advance(15);
    CHECK_EQ(harness.host.titles, 1);
    CHECK_EQ(harness.host.restarts, 1);
    CHECK(harness.pause->error.empty());
}

TEST(game_over_disables_continue) {
    PauseHarness harness;
    CHECK(harness.pause->open(PauseEntrance::GameOver));
    CHECK_EQ(int(harness.pause->state.screen), int(PauseScreen::GameOver));
    harness.advance(12);
    CHECK_EQ(harness.pause->state.menu.count, 3);
    CHECK_EQ(harness.pause->state.menu.cursor, 1);  // 落在重开（继续被禁用）
    for (int i = 0; i < 6; i++) harness.frame(0x10);  // 充分上移也回不到 0
    const i32 cursor = harness.pause->state.menu.cursor;
    CHECK(cursor == 1 || cursor == 2);
    harness.frame(1);
    harness.advance(15);
    CHECK_EQ(harness.host.restarts + harness.host.titles, 1);
}

int main() { return th13test::run_all("暂停覆盖层"); }
