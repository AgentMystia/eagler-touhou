// 对局场景（Stage1 可玩化：HUD + StageBattle/StagePresentation + STD 舞台背景 +
// 帧钟 + Esc/失焦暂停请求）。ECL/ANM 数据驱动见 StageBattle/StagePresentation，
// 背景由 stXX.std/stXXwl.anm 驱动见 StageScenery；状态机合同见
// docs/plans/ui-base.md §四。
#pragma once

#include "GameHud.hpp"
#include "HudDrawData.hpp"
#include "HudFrameData.hpp"
#include "MsgPlayer.hpp"
#include "PostGameFlow.hpp"
#include "SessionState.hpp"
#include "StageBattle.hpp"
#include "StagePresentation.hpp"
#include "StageScenery.hpp"
#include "FrameScheduler.hpp"
#include "AnmDrawSchedule.hpp"
#include "ScreenViews.hpp"

namespace th13 {

struct RunSceneServices : HudDrawServices, HudFrameServices, StageBattleServices,
                          AnmDrawServices {
    virtual bool request_pause() = 0;
    // 对局终了链路的场景请求（raw = DAT_004dcc24 真值：0xc 下一关 / 0xf 结局 /
    // 4 标题 / 0xa 重试…全表见 results-flow.md §1）。
    // Application 层接线 TODO：本阶段默认空实现（请求被记录但不生效）。
    virtual bool request_destination(i32 raw) {
        (void)raw;
        return false;
    }
    // 交互装载：ECL/ANM 字节（经宿主资源通道，浏览器侧 /assets）。
    virtual bool read_asset(const std::string& name, std::vector<u8>& bytes, bool required = true) = 0;
    // STD 舞台背景支撑：图形设备 / 背景 ANM bank 装载（宿主侧 preload 一体）。
    virtual ZunGraphics& stage_graphics() = 0;
    virtual bool preload_scenery_animation(i32 bank, const std::string& name) = 0;
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

class RunScene final : private StageDialogHost, private MsgHost, private PostGamePlatform {
    SessionState& progress;
    AnmManager& animations;
    AnmRenderer& scene_renderer;
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
    std::array<StageDraw, 2> scenery_draws;  // STD 背景（背景组/前景组）
    i32 scenery_bank = -1;
    std::string scenery_std;
    // Boss 登场变暗：boss 槽存活期间游戏区黑幕淡入（golden 2450 实证全暗
    // ≈0.6 α，边带不暗）；2381（MBoss spawn+setBoss 同帧）未见暗 → 30 帧 ramp。
    i32 boss_dim = 0;
    // S3 对局终了链路：msg 播放器（Boss 前后对话）+ 对局内覆盖层骨架。
    MsgFile dialog_file;
    MsgPlayer dialog;
    bool dialog_ready = false;  // msg 资产缺失时降级为对话缺席（519 即过）
    PostGameFlow post_flow;
    bool fail(const std::string&);

public:
    // 自机弹精灵（plXX.anm 内 16×16 御札帧；.sht 弹型表接入前的可玩化近似）。
    static constexpr i32 kPlayerShotSprite = 31;
    GameHud hud;
    StageBattle battle;
    std::unique_ptr<StagePresentation> presentation;
    std::unique_ptr<StageScenery> scenery;  // STD 舞台背景（stXX.std 驱动）
    std::string battle_sht;  // 移速源 .sht（回放会话注入；空 = 模型默认 pl02a 定标）
    u32 input_held = 0, input_pressed = 0, input_repeated = 0;
    Timer age{0, 0, 0, 0, 0};
    std::string error;
    RunScene(SessionState&, HudPlayerState&, HudScoreState&, AnmManager&, AnmRenderer&,
             AnmEnvironment&, RunSceneServices&, FrameScheduler&,
             StagePresentation::Banks banks, i32 front_bank, i32 ascii_bank,
             i32 scenery_bank, const std::string& std_name);
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
    bool draw_scenery(u32 pass);  // STD 背景（层 0-7 背景组 / 8-11 前景组）
    void request_pause() { host.request_pause(); }
    void request_game_over() { host.request_game_over(); }

private:
    // StageDialogHost（ins_518/519 接口位）
    void open_dialog(i32 entry) override;
    bool dialog_active() const override { return dialog.active(); }
    // MsgHost（msg opcode 回调；21 → 0x42cd40 关终了）
    void msg_bgm(i32 opcode) override;         // BGM 族 TODO（音频接线）
    void msg_portrait_load() override;         // 立绘 bank TODO（视觉阶段）
    void msg_boss_battle() override;           // 0x43af70 TODO（Boss 设置接线）
    void msg_stage_clear() override;           // → PostGameFlow::on_stage_clear
    // PostGamePlatform
    void request_destination(i32 raw) override { host.request_destination(raw); }
    void bgm(i32 id) override { msg_bgm(id); }
};

}  // namespace th13
