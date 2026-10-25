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
#include "../game/ReplaySession.hpp"
#include "../game/TitleScene.hpp"
#include "../game/TitleMusicRoomMenu.hpp"
#include "../game/TitleOptionsMenu.hpp"
#include "../game/RunScene.hpp"
#include "../game/RunPause.hpp"
#include "../game/PlayfieldTargets.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace th13::sdl {

// 原版 supervisor destination 语义（数值沿用 th15 对照：13 开局、2/4/14 回标题
// 变体、3 退出、15 结局；th13 侧锚定后在 AddressMap.hpp 登记回填）。
// NextStage = th13 DAT_004dcc24 的 0xc 换关请求（0x42cd40 关 1-5 主干）——
// 延续对局、按已递增的 progress.stage 装载，与 13 的全新开局区分。
enum class Destination : i32 {
    Quit = 3,
    Title = 4,
    TitleFromGameOver = 2,
    TitleFromReplay = 14,
    Run = 13,
    NextStage = 100,
    Ending = 15
};

struct ApplicationState final : TitleScenePlatform, RunSceneServices, RunPausePlatform,
                                TitleOptionPlatform, TitleMusicRoomPlatform {
    GraphicsDevice graphics;
    FontDevice fonts{graphics};
    // 字体设备初始化失败的显式记录（原版 MS Gothic 启动资产缺席 → 对话日文
    // 渲染 BLOCKED；空 = 字体可用）。标题/HUD ASCII 文本不依赖字体设备。
    std::string fonts_failure;
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
    // 0x4dc324 等（game RNG）：ECL gvars RAND 族（0x420380/0x420d00）、敌机
    // 指令（0x41ce10/0x417b10）、弹幕出生/etEx（0x40e840/0x40f030）、自机弹
    // 出膛（0x445c40）共用此对象；0x4dc31c（visual）是 ANM 指令/畸变侧的
    // 独立第二对象——反汇编逐调用点核验见 artifacts/verification/r3/R。
    // StageSim/StageBattle 消费此实例的构造注入属 StageBattle 所有者（I）
    // 的接线（R3 已在 begin_replay_run 恢复本实例种子）。
    GameRandom game_random;
    std::unique_ptr<SceneDisplay> display;
    std::array<FrameCallback, 2> animation_updates;
    std::unique_ptr<TitleScene> title;
    std::unique_ptr<RunScene> run;
    std::unique_ptr<RunPause> pause;
    // Original two-surface game composition; no gameplay-coordinate shortcut.
    std::unique_ptr<PlayfieldTargets> playfield_targets;
    i32 pending_destination = -1, selected_stage = 1, current_destination = 13;
    u32 frames = 0;
    // 回放调试会话（th13_debug_* 帧泵驱动；正常标题流不经过）。输入流以
    // 0x448200 节奏逐帧接管 controls——装载态/难度/角色取自录像头，直入对局。
    ReplayFile replay_file;
    std::unique_ptr<ReplayInputCursor> replay_cursor;
    bool replay_active = false;
    u32 replay_consumed = 0;
    // 回放恢复挂点：begin_replay_run 备好 stage 指针，begin_run 在关首表
    // 重置（0x42b990:103-110 等价）之后、场景构造/ECL 第 0 帧之前应用
    // 0x449070 恢复（种子 + 22 dword）。正常开局/换关为空。
    const ReplayStage* pending_replay_stage = nullptr;
    u32 pending_replay_stage_index = 0;
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
    // 唯一 GameInput 实例逐帧传入（BrowserRuntime 采样；00448200/00414f30 相位）。
    bool step(const GameInput& input, float fps, bool lost_focus);
    bool apply_destination();
    bool begin_title(i32 reason);
    bool begin_run();
    // 回放会话直入：解码 .rpy → 按录像头配置 → begin_run（跳过标题与装载挡板）。
    bool begin_replay_run(const u8* data, size_t size, u32 stage_index);
    u32 replay_take_input();  // 每游戏帧恰一次：取当前输入态并推进游标
    void release_run();
    const i32* projection();
    bool read(const std::string&, std::vector<u8>&, bool required = true);
    // ---- [R3-M] 标题 Replay 菜单（TitleReplayPlatform 实现，0x452300 槽位语义）----
    // 槽 0-24 = 编号（导入名 th13_NN.rpy 同名落槽）、25-73 = 导入追加、
    // 74 = 网页「导入本地 .rpy」入口行（占用原版第 50 用户槽，网页保留）。
    // decoded 供菜单借用（只读），bytes 供 start_replay 帧尾交给生产解码
    // begin_replay_run（0x448c40 装载序重走，不建平行状态）。
    struct ReplayCatalogSlot {
        std::string name;
        std::vector<u8> bytes;
        std::shared_ptr<ReplayFile> decoded;
    };
    std::array<ReplayCatalogSlot, 75> replay_slots{};
    std::string replay_import_error_;
    i32 pending_replay_file = -1, pending_replay_stage_slot = -1;  // start_replay 排队
    bool import_replay_bytes(const u8* data, size_t size, const std::string& name);
    // TitleReplayPlatform
    i32 replay_count() const override;
    const ReplayFile* replay_entry(i32 index) const override;
    std::string replay_filename(i32 index) const override;
    bool request_replay_file() override;
    std::string replay_import_error() const override;
    bool start_replay(i32 file_index, i32 stage_index) override;
    // TitleScenePlatform
    bool clear_title_overlay() override;
    bool queue_title_music(i32 code, i32 value, const std::string&) override;
    bool clear_current_wave() override;
    bool title_exit(i32 destination) override;
    bool fade_out_title_music() override;
    bool sound(i32) override;
    bool begin_transition(u32&) override;
    bool start_game(i32 stage) override;
    bool request_destination(i32 raw);  // supervisor 0x4dcc24 真值 → destination
    bool hud_text(const HudTextDraw&) override;
    // TitleMenuServices：clear-tag 真实存档门（0x451450 case0 score 块
    // +0x56dc*char+0x7e4+diff*4 != 0；MenuExtras FileStore 数据源，缺档全隐）。
    bool character_cleared(i32 character, i32 difficulty) override;
    // TitleOptionPlatform（Options 屏宿主面，r4/menu-extras 契约 §三）
    void apply_volumes(i32 bgm, i32 effect) override;
    bool persist_options(const TitleOptionVolumes& volumes) override;
    bool open_key_config() override;  // 键配置屏（0x44fcc0 screen 4）分期后接
    // TitleMusicRoomPlatform（musicroom-host.md 八回调；r4/menu-extras §六）
    i32 track_count() const override;
    std::string track_title(i32 track) override;
    std::string track_comment(i32 track, i32 line) override;
    bool track_heard(i32 track) const override;
    void mark_heard(i32 track) override;      // 0x4554f0 Z 路径落档
    bool play_track(i32 track) override;      // 0x43adc0(0,曲名)+0x461830(2,0)
    bool play_locked_response() override;     // 0x461830(3/4,0)
    bool resume_title_music() override;       // 0x43adc0(0,"th13_00")
    // MusicRoom 曲目目录（musiccmt.txt '@bgm/名' 块 = 播放名 + 曲名行 + 8
    // 注释行；cp932 字节原样，显示归菜单/字体通道，不在此转码）。空 = 资产
    // 缺席（菜单如实空目录，不伪造曲目）。
    struct MusicRoomTrack {
        std::string name;
        std::string title;
        std::array<std::string, 8> comment;
    };
    std::vector<MusicRoomTrack> music_room;
    void load_music_room_catalog();
    // RunSceneServices
    bool request_pause() override;
    bool request_game_over() override;
    bool hud_sound(i32) override;
    bool read_asset(const std::string& name, std::vector<u8>& bytes, bool required = true) override {
        return read(name, bytes, required);
    }
    bool camera(DrawCamera index, bool refresh) override { return views.camera(index, refresh); }
    ZunGraphics& stage_graphics() override { return graphics; }
    bool preload_scenery_animation(i32 bank, const std::string& name) override;
    bool queue_music(i32 code, i32 value, const std::string& name) override {
        return audio_device.queue_music(code, value, name) || fail(audio_device.error);
    }
    // RunPausePlatform
    bool suspend_music() override;
    bool resume_music() override;
    bool restart_run() override;
    bool return_to_title() override;
    // 对局期平台服务（battle 分期前的 HUD 直通；StageBattleServices 直通面）
    bool life_hud(i32, i32) override;
    bool bomb_hud(i32, i32) override;
    bool game_over();
};

}  // namespace th13::sdl
