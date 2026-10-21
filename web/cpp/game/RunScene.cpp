#include "RunScene.hpp"

namespace th13 {

namespace {
// 场景回调优先级（th13 原版调度位待锚定；先按 th15 同族布局取值）：
// 更新 20（标题 6 与暂停 11 之间），HUD 文本 76（动画层 75 之后、字幕 78 之前）。
// 战斗直绘穿插 AnmDrawSchedule 各层：自机 12（层 4 区）/自机弹 17（层 6 区）/
// 敌机 19（层 8 同拍）/敌弹 43（层 20 之后，注册晚于动画层回调故同值仍靠后）。
constexpr i32 kRunUpdatePriority = 20;
constexpr i32 kRunTextPriority = 76;
constexpr std::array<i32, 4> kStageDrawPriorities = {12, 17, 19, 43};
}  // namespace

RunScene::RunScene(SessionState& p, HudPlayerState& player, HudScoreState& score,
                   AnmManager& animations, AnmRenderer& scene_renderer, AnmEnvironment& e,
                   RunSceneServices& h, FrameScheduler& f, StagePresentation::Banks banks,
                   i32 front, i32 ascii)
    : progress(p),
      environment(e),
      host(h),
      scheduler(f),
      player_state(player),
      score_state(score),
      hud(animations, p, player, score, front, ascii),
      presentation(std::make_unique<StagePresentation>(animations, scene_renderer, banks,
                                                       kPlayerShotSprite)) {
    age.set(0);
    callbacks[0].owner = callbacks[1].owner = this;
    callbacks[0].enabled = callbacks[1].enabled = true;
    callbacks[0].run = [](void* p) {
        return static_cast<RunScene*>(p)->update() ? i32(FrameAction::Continue)
                                                   : i32(FrameAction::Error);
    };
    callbacks[1].run = [](void* p) {
        return static_cast<RunScene*>(p)->draw() ? i32(FrameAction::Continue)
                                                 : i32(FrameAction::Error);
    };
    if (scheduler.add(callbacks[0], FramePass::Update, kRunUpdatePriority) < 0)
        fail("Run scene callback registration failed");
    if (scheduler.add(callbacks[1], FramePass::Draw, kRunTextPriority) < 0)
        fail("Run scene text callback registration failed");
}

RunScene::~RunScene() {
    for (auto& callback : callbacks) scheduler.remove(callback);
    for (auto& draw : stage_draws) scheduler.remove(draw.callback);
    if (presentation) presentation->release();
}

bool RunScene::fail(const std::string& why) {
    if (error.empty()) error = why.empty() ? "Run scene unavailable" : why;
    return false;
}

bool RunScene::initialize() {
    if (!error.empty()) return false;
    // 战斗装载：ECL 交互模式 + 首帧（root 创建趟）。难度位 = 1 << 难度序号
    // （rank 掩码 1111LHNE；契约默认 0x08 = Lunatic）。
    RunSceneAssets provider;
    provider.host = &host;
    provider.sim = &battle.sim;
    provider.anim_list = &battle.sim.anim_list;
    if (!battle.load(provider, "st01.ecl", 1 << (progress.difficulty & 3), host, progress,
                     player_state, score_state))
        return fail(battle.error().empty() ? "Stage battle load failed" : battle.error());
    for (u32 index = 0; index < stage_draws.size(); index++) {
        auto& draw = stage_draws[index];
        draw.owner = this;
        draw.pass = index;
        draw.callback.owner = &draw;
        draw.callback.enabled = true;
        draw.callback.run = [](void* p) -> i32 {
            auto* draw = static_cast<StageDraw*>(p);
            return draw->owner->draw_stage(draw->pass) ? i32(FrameAction::Continue)
                                                       : i32(FrameAction::Error);
        };
        if (scheduler.add(draw.callback, FramePass::Draw, kStageDrawPriorities[index]) < 0)
            return fail("Stage draw callback registration failed");
    }
    hud.prepare_stage_assets();
    if (!hud.initialize(HudEntryContext{})) return fail(hud.error);
    return true;
}

bool RunScene::update() {
    if (!error.empty()) return false;
    if (environment.paused) return true;  // 暂停覆盖层冻结对局（th15 environment.paused 同机制）
    progress.stage_frame++;
    age.tick(&progress.rate);
    GameInput input;
    input.held = input_held;
    input.pressed = input_pressed;
    input.repeated = input_repeated;
    if (!battle.step(input)) return fail(battle.error());
    if (!presentation->sync(battle, input_held)) return fail(presentation->error());
    hud.update(HudFrameContext{}, host);
    hud.update_score();
    return hud.error.empty() || fail(hud.error);
}

bool RunScene::draw_stage(u32 pass) {
    if (!error.empty()) return false;
    if (!host.camera(DrawCamera::Fullscreen, false)) return fail("Stage camera unavailable");
    if (!presentation->draw(pass)) return fail(presentation->error());
    return true;
}

bool RunScene::draw() {
    if (!error.empty()) return false;
    hud.draw(HudDrawContext{}, host);
    return hud.error.empty() || fail(hud.error);
}

}  // namespace th13
