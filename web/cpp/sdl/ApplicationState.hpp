// Application 聚合根（th15 ApplicationState 裁剪：destination 驱动的场景所有权，
// 装载挡板与 16 槽 projection 保留；practice/checkpoint/ending/replay 不做，
// docs/plans/ui-base.md §四）。
#pragma once

#include "Application.hpp"
#include "SceneDisplay.hpp"
#include "FontDevice.hpp"
#include "AudioDevice.hpp"
#include "FileStore.hpp"
#include "../game/ReplayFile.hpp"
#include "../game/TitleScene.hpp"
#include "../game/RunScene.hpp"
#include "../game/RunPause.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace th13::sdl {

// 原版 supervisor destination 语义（数值沿用 th15 对照：13 开局、2/4/14 回标题
// 变体、3 退出、15 结局；th13 侧锚定后在 AddressMap.hpp 登记回填）。
enum class Destination : i32 { Quit = 3, Title = 4, TitleFromGameOver = 2, TitleFromReplay = 14, Run = 13, Ending = 15 };

struct ApplicationState final : TitleScenePlatform, RunSceneServices, RunPausePlatform {
    GraphicsDevice graphics;
    FontDevice fonts{graphics};
    AudioDevice audio_device;
    FileStore files;
    GameRandom visual;
    AnmEnvironment environment;
    AnmManager animations{visual, environment};
    AnmRenderer renderer{graphics};
    ScreenViews views{renderer, environment};
    AsciiText captions{animations, environment};
    FrameScheduler scheduler;
    GameRandom loading_random;
    AnmEnvironment loading_environment;
    AnmManager loading_animations{loading_random, loading_environment};
    bool platform_prepared = false;
    u32 loading_signature = 0, loading_prayer = 0;
    unsigned pending_load = 0, loading_frames = 0;
    bool loading_startup = false;
    double loading_debt = 0;
    SessionState progress;
    HudPlayerState selection_player;
    HudScoreState selection_score;
    TitleSelectionSettings selection;
    std::unique_ptr<SceneDisplay> display;
    std::array<FrameCallback, 2> animation_updates;
    std::unique_ptr<TitleScene> title;
    std::unique_ptr<RunScene> run;
    std::unique_ptr<RunPause> pause;
    i32 pending_destination = -1, selected_stage = 1, current_destination = 13;
    u32 frames = 0;
    // 回放调试会话（th13_debug_* 帧泵驱动；正常标题流不经过）。输入流以
    // 0x448200 节奏逐帧接管 controls——装载态/难度/角色取自录像头，直入对局。
    ReplayFile replay_file;
    std::unique_ptr<ReplayInputCursor> replay_cursor;
    std::string replay_sht;  // plXXa/b.sht（§7.2 移速源；页面侧随录像备入 /assets）
    bool replay_active = false;
    u32 replay_consumed = 0;
    bool initialized = false, exiting = false;
    std::array<i32, 16> projected{};
    std::string failure;
    std::unordered_map<std::string, std::vector<u8>> resource_cache;

    bool loading_waiting() const;
    bool complete_loading();
    bool render_loading();
    bool advance_loading(double);
    bool fail(const std::string&);
    bool prepare_platform();
    bool prepare_loading();
    bool draw_loading(bool signature);
    bool initialize(bool);
    ~ApplicationState();
    bool step(u32, u32, u32, float, bool);
    bool apply_destination();
    bool begin_title(i32 reason);
    bool begin_run();
    // 回放会话直入：解码 .rpy → 按录像头配置 → begin_run（跳过标题与装载挡板）。
    bool begin_replay_run(const u8* data, size_t size, u32 stage_index);
    u32 replay_take_input();  // 每游戏帧恰一次：取当前输入态并推进游标
    void release_run();
    const i32* projection();
    bool read(const std::string&, std::vector<u8>&);
    // TitleScenePlatform
    bool clear_title_overlay() override;
    bool queue_title_music(i32 code, i32 value, const std::string&) override;
    bool clear_current_wave() override;
    bool title_exit(i32 destination) override;
    bool fade_out_title_music() override;
    bool sound(i32) override;
    bool begin_transition(u32&) override;
    bool start_game(i32 stage) override;
    bool hud_text(const HudTextDraw&) override;
    // RunSceneServices
    bool request_pause() override;
    bool request_game_over() override;
    bool hud_sound(i32) override;
    bool read_asset(const std::string& name, std::vector<u8>& bytes) override {
        return read(name, bytes);
    }
    bool camera(DrawCamera index, bool refresh) override { return views.camera(index, refresh); }
    // RunPausePlatform
    bool suspend_music() override;
    bool restart_run() override;
    bool return_to_title() override;
    // 对局期平台服务（battle 分期前的 HUD 直通；StageBattleServices 直通面）
    bool life_hud(i32, i32) override;
    bool bomb_hud(i32, i32) override;
    bool game_over();
};

}  // namespace th13::sdl
