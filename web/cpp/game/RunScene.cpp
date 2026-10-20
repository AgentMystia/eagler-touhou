#include "RunScene.hpp"

namespace th13 {

namespace {
// 场景回调优先级（th13 原版调度位待锚定；先按 th15 同族布局取值）：
// 更新 20（标题 6 与暂停 11 之间），HUD 文本 76（动画层 75 之后、字幕 78 之前）。
constexpr i32 kRunUpdatePriority = 20;
constexpr i32 kRunTextPriority = 76;
}  // namespace

RunScene::RunScene(SessionState& p, HudPlayerState& player, HudScoreState& score,
                   AnmManager& animations, AnmEnvironment& e, RunSceneServices& h,
                   FrameScheduler& f, i32 front, i32 ascii)
    : progress(p), environment(e), host(h), scheduler(f), hud(animations, p, player, score, front, ascii) {
    age.set(0);
    callbacks[0].owner = callbacks[1].owner = this;
    callbacks[0].enabled = callbacks[1].enabled = true;
    callbacks[0].run = [](void* p) {
        return static_cast<RunScene*>(p)->update() ? i32(FrameAction::Continue) : i32(FrameAction::Error);
    };
    callbacks[1].run = [](void* p) {
        return static_cast<RunScene*>(p)->draw() ? i32(FrameAction::Continue) : i32(FrameAction::Error);
    };
    if (scheduler.add(callbacks[0], FramePass::Update, kRunUpdatePriority) < 0)
        fail("Run scene callback registration failed");
    if (scheduler.add(callbacks[1], FramePass::Draw, kRunTextPriority) < 0)
        fail("Run scene text callback registration failed");
}

RunScene::~RunScene() {
    for (auto& callback : callbacks) scheduler.remove(callback);
}

bool RunScene::fail(const std::string& why) {
    if (error.empty()) error = why.empty() ? "Run scene unavailable" : why;
    return false;
}

bool RunScene::initialize() {
    if (!error.empty()) return false;
    hud.prepare_stage_assets();
    return hud.initialize(HudEntryContext{}) || fail(hud.error);
}

bool RunScene::update() {
    if (!error.empty()) return false;
    if (environment.paused) return true;  // 暂停覆盖层冻结对局（th15 environment.paused 同机制）
    progress.stage_frame++;
    age.tick(&progress.rate);
    hud.update(HudFrameContext{}, host);
    hud.update_score();
    return hud.error.empty() || fail(hud.error);
}

bool RunScene::draw() {
    if (!error.empty()) return false;
    hud.draw(HudDrawContext{}, host);
    return hud.error.empty() || fail(hud.error);
}

}  // namespace th13
