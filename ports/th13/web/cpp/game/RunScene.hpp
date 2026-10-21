// 对局场景（Stage1 可玩化：HUD + StageBattle/StagePresentation + 帧钟 +
// Esc/失焦暂停请求）。ECL/ANM 数据驱动见 StageBattle/StagePresentation；
// 状态机合同见 docs/plans/ui-base.md §四。
#pragma once

#include "GameHud.hpp"
#include "HudDrawData.hpp"
#include "HudFrameData.hpp"
#include "SessionState.hpp"
#include "StageBattle.hpp"
#include "StagePresentation.hpp"
#include "FrameScheduler.hpp"
#include "AnmDrawSchedule.hpp"

namespace th13 {

struct RunSceneServices : HudDrawServices, HudFrameServices, StageBattleServices,
                          AnmDrawServices {
    virtual bool request_pause() = 0;
    // 交互装载：ECL/ANM 字节（经宿主资源通道，浏览器侧 /assets）。
    virtual bool read_asset(const std::string& name, std::vector<u8>& bytes) = 0;
};

// 交互装载供给：read_asset → EclResourceProvider（ECL 递归 include + ANIM 名单）。
class RunSceneAssets final : public EclResourceProvider {
public:
    RunSceneServices* host = nullptr;
    StageSim* sim = nullptr;
    std::vector<std::string>* anim_list = nullptr;
    bool read(const std::string& name, std::vector<u8>& data) override {
        return host && host->read_asset(name, data);
    }
    bool animation(u32 slot, const std::string& name) override {
        // 装载核验（0x4795b0 注册序的行为子集）：ANM 必须可读；slot 从 10 起
        // 记名——ECL 302 引用的文件槽 = 2 + 名单序（st01：2=enemy、3=st01enm）。
        std::vector<u8> probe;
        if (!host || !host->read_asset(name, probe)) {
            if (sim) sim->error_ = "missing ANM dependency: " + name;
            return false;
        }
        (void)slot;
        if (anim_list) anim_list->push_back(name);
        return true;
    }
};

class RunScene {
    SessionState& progress;
    AnmEnvironment& environment;
    RunSceneServices& host;
    FrameScheduler& scheduler;
    HudPlayerState& player_state;
    HudScoreState& score_state;
    std::array<FrameCallback, 2> callbacks;
    struct StageDraw {
        FrameCallback callback;
        RunScene* owner = nullptr;
        u32 pass = 0;
    };
    std::array<StageDraw, 4> stage_draws;  // 自机/自机弹/敌机/敌弹直绘
    bool fail(const std::string&);

public:
    // 自机弹精灵（plXX.anm 内 16×16 御札帧；.sht 弹型表接入前的可玩化近似）。
    static constexpr i32 kPlayerShotSprite = 31;
    GameHud hud;
    StageBattle battle;
    std::unique_ptr<StagePresentation> presentation;
    std::string battle_sht;  // 移速源 .sht（回放会话注入；空 = 模型默认 pl02a 定标）
    u32 input_held = 0, input_pressed = 0, input_repeated = 0;
    Timer age{0, 0, 0, 0, 0};
    std::string error;
    RunScene(SessionState&, HudPlayerState&, HudScoreState&, AnmManager&, AnmRenderer&,
             AnmEnvironment&, RunSceneServices&, FrameScheduler&,
             StagePresentation::Banks banks, i32 front_bank, i32 ascii_bank);
    ~RunScene();
    RunScene(const RunScene&) = delete;
    RunScene& operator=(const RunScene&) = delete;
    void controls(u32 held, u32 pressed, u32 repeated) {
        input_held = held;
        input_pressed = pressed;
        input_repeated = repeated;
    }
    bool initialize();
    bool update();
    bool draw();
    bool draw_stage(u32 pass);  // 战斗直绘（initialize 注册的 4 个 Draw 回调）
    void request_pause() { host.request_pause(); }
    void request_game_over() { host.request_game_over(); }
};

}  // namespace th13
