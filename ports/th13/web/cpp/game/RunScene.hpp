// 对局场景（UI 基座最小对局宿主：HUD + 帧钟 + Esc/失焦暂停请求；
// Stage1 场景资产/ECL/敌机按 anm-pipeline 与 ecl-vm 分期接入，ui-base.md §四）。
#pragma once

#include "GameHud.hpp"
#include "HudDrawData.hpp"
#include "HudFrameData.hpp"
#include "SessionState.hpp"
#include "FrameScheduler.hpp"

namespace th13 {

struct RunSceneServices : HudDrawServices, HudFrameServices {
    virtual bool request_pause() = 0;
    virtual bool request_game_over() = 0;
};

class RunScene {
    SessionState& progress;
    AnmEnvironment& environment;
    RunSceneServices& host;
    FrameScheduler& scheduler;
    std::array<FrameCallback, 2> callbacks;
    bool fail(const std::string&);

public:
    GameHud hud;
    Timer age{0, 0, 0, 0, 0};
    std::string error;
    RunScene(SessionState&, HudPlayerState&, HudScoreState&, AnmManager&, AnmEnvironment&,
             RunSceneServices&, FrameScheduler&, i32 front_bank, i32 ascii_bank);
    ~RunScene();
    RunScene(const RunScene&) = delete;
    RunScene& operator=(const RunScene&) = delete;
    bool initialize();
    bool update();
    bool draw();
    void request_pause() { host.request_pause(); }
    void request_game_over() { host.request_game_over(); }
};

}  // namespace th13
