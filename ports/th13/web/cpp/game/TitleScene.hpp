// 标题场景（th15 TitleScene 裁剪：全部原版标题屏共享一份光标历史、动画注册表
// 与回调调度器；平台拥有文件/音频/GPU 工作并排队 destination）。
#pragma once

#include "TitleFrame.hpp"
#include "TitleMainMenu.hpp"
#include "TitleReplayMenu.hpp"
#include "TitleSelectionMenu.hpp"
#include "TitleOptionsMenu.hpp"
#include "TitleMusicRoomMenu.hpp"
#include "TitleManualMenu.hpp"
#include "HudDrawData.hpp"

namespace th13 {

// 平台聚合：Replay 目录/导入/启动 + Option 音量/持久化 + 音楽室曲目/听过位
// + Manual help 查看器服务随场景平台一并注入（TitleReplayPlatform/
// TitleOptionPlatform/TitleMusicRoomPlatform/TitleManualPlatform 全默认实现，
// 空宿主不受影响；数据源见 r4/menu-extras/integration.md）。
struct TitleScenePlatform : TitleFramePlatform, TitleReplayPlatform, TitleOptionPlatform,
                            TitleMusicRoomPlatform, TitleManualPlatform, TitleMenuServices,
                            HudDrawServices {
    virtual bool sound(i32) = 0;
};

class TitleScene final : private TitleFrameServices, private TitleMenuServices {
public:
    TitleState state;
    std::string error;

private:
    SessionState& progress;
    TitleSelectionSettings& settings;
    AnmManager& animations;
    TitleScenePlatform& host;
    FrameScheduler& scheduler;
    TitleSceneResources resources;
    TitleFrameInput input;
    TitleMainMenu main;
    TitleDifficultyMenu difficulty;
    TitleCharacterMenu character;
    TitleReplayMenu replay;
    TitleOptionVolumes option_volumes;  // 音量真值（宿主 FileStore 预填，菜单共享编辑）
    TitleOptionsMenu option;
    TitleMusicRoomMenu music_room;
    TitleManualMenu manual;
    bool check(bool, const std::string&);
    // TitleFrameServices
    bool clear_title_overlay() override;
    bool queue_title_music(i32 code, i32 value, const std::string&) override;
    bool clear_current_wave() override;
    bool title_exit(i32) override;
    bool fade_out_title_music() override;
    bool update_title_menu(TitleScreen, const TitleFrameInput&) override;
    void advance_main_menu_selection() override;
    bool draw_title_replay_texts() override;
    bool draw_title_music_room_texts() override;
    // TitleMenuServices
    bool sound(i32) override;
    bool begin_transition(u32&) override;
    bool start_game(i32) override;
    bool character_cleared(i32 character, i32 difficulty) override;

public:
    // Option 屏观测（row/volumes/error 采样；接线与测试用，不提供可变访问）。
    const TitleOptionsMenu& option_menu() const noexcept { return option; }
    // 音楽室观测（row/error 采样）。
    const TitleMusicRoomMenu& music_room_menu() const noexcept { return music_room; }
    // マニュアル观测（error 采样；wrapper≠真实 help viewer 完成）。
    const TitleManualMenu& manual_menu() const noexcept { return manual; }
    TitleFrame frame;
    TitleScene(SessionState&, TitleSelectionSettings&, AnmManager&, TitleScenePlatform&,
               FrameScheduler&, TitleSceneResources = {});
    ~TitleScene();
    TitleScene(const TitleScene&) = delete;
    TitleScene& operator=(const TitleScene&) = delete;
    void controls(const TitleFrameInput& value) noexcept {
        input = value;
        frame.controls(value);
    }
};

}  // namespace th13
