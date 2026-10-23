// 标题场景（th15 TitleScene 裁剪：全部原版标题屏共享一份光标历史、动画注册表
// 与回调调度器；平台拥有文件/音频/GPU 工作并排队 destination）。
#pragma once

#include "TitleFrame.hpp"
#include "TitleMainMenu.hpp"
#include "TitleSelectionMenu.hpp"
#include "HudDrawData.hpp"

namespace th13 {

struct TitleScenePlatform : TitleFramePlatform, TitleMenuServices, HudDrawServices {
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
    bool check(bool, const std::string&);
    // TitleFrameServices
    bool clear_title_overlay() override;
    bool queue_title_music(i32 code, i32 value, const std::string&) override;
    bool clear_current_wave() override;
    bool title_exit(i32) override;
    bool fade_out_title_music() override;
    bool update_title_menu(TitleScreen, const TitleFrameInput&) override;
    void advance_main_menu_selection() override;
    // TitleMenuServices
    bool sound(i32) override;
    bool begin_transition(u32&) override;
    bool start_game(i32) override;

public:
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
