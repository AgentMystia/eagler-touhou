#include "RunScene.hpp"

#include <cstdio>

namespace th13 {

namespace {
// 场景回调优先级（th13 原版调度位待锚定；先按 th15 同族布局取值）：
// 更新 20（标题 6 与暂停 11 之间），HUD 文本 76（动画层 75 之后、字幕 78 之前）。
// 战斗直绘穿插 AnmDrawSchedule 各层：自机 12（层 4 区）/自机弹 17（层 6 区）/
// 敌机 19（层 8 同拍）/敌弹 43（层 20 之后，注册晚于动画层回调故同值仍靠后）。
// STD 舞台背景两组均在动画层 0（优先级 5）之前：背景组 3 / 前景组 4
// （th15 StageDrawFrame 3/6 的 th13 排位：层 0 前置约束下压缩到 3/4）。
constexpr i32 kRunUpdatePriority = 20;
constexpr i32 kRunTextPriority = 76;
constexpr std::array<i32, 4> kStageDrawPriorities = {12, 17, 19, 43};
constexpr std::array<i32, 2> kSceneryDrawPriorities = {3, 4};
}  // namespace

RunScene::RunScene(SessionState& p, HudPlayerState& player, HudScoreState& score,
                   AnmManager& manager, AnmRenderer& scene_view, AnmEnvironment& e,
                   RunSceneServices& h, FrameScheduler& f, StagePresentation::Banks banks,
                   i32 front, i32 ascii, i32 scenery_bank, const std::string& std_name)
    : progress(p),
      animations(manager),
      scene_renderer(scene_view),
      environment(e),
      host(h),
      scheduler(f),
      player_state(player),
      score_state(score),
      hud(animations, p, player, score, front, ascii),
      presentation(std::make_unique<StagePresentation>(animations, scene_renderer, banks,
                                                       kPlayerShotSprite)),
      scenery_bank(scenery_bank),
      scenery_std(std_name),
      post_flow(p, player, score, *this) {
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
    for (u32 index = 0; index < scenery_draws.size(); index++) {
        auto& draw = scenery_draws[index];
        draw.owner = this;
        draw.pass = index;
        draw.callback.owner = &draw;
        draw.callback.enabled = true;
        draw.callback.run = [](void* p) -> i32 {
            auto* draw = static_cast<StageDraw*>(p);
            return draw->owner->draw_scenery(draw->pass) ? i32(FrameAction::Continue)
                                                         : i32(FrameAction::Error);
        };
        if (scheduler.add(draw.callback, FramePass::Draw, kSceneryDrawPriorities[index]) < 0)
            fail("Scenery draw callback registration failed");
    }
}

RunScene::~RunScene() {
    for (auto& callback : callbacks) scheduler.remove(callback);
    for (auto& draw : stage_draws) scheduler.remove(draw.callback);
    for (auto& draw : scenery_draws) scheduler.remove(draw.callback);
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
                     player_state, score_state, nullptr,
                     battle_sht.empty() ? nullptr : battle_sht.c_str()))
        return fail(battle.error().empty() ? "Stage battle load failed" : battle.error());
    // STD 舞台背景装载：stXX.std 解析 → 引用 ANM（st01wl.anm）入 bank → 图元
    // VM 池绑定（模板复制 + 首 tick）。
    std::vector<u8> bytes;
    if (!host.read_asset(scenery_std, bytes)) return fail("Stage STD missing: " + scenery_std);
    scenery = std::make_unique<StageScenery>();
    if (!scenery->file.open(bytes.data(), u32(bytes.size())))
        return fail("Stage STD invalid: " + scenery->file.error);
    if (!host.preload_scenery_animation(scenery_bank, scenery->file.animation_name))
        return fail("Stage scenery ANM unavailable: " + scenery->file.animation_name);
    if (!scenery->initialize(animations, scenery_bank))
        return fail(scenery->error.empty() ? "Stage scenery init failed" : scenery->error);
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
    // S3：对话资产（st0Na-d.msg 按角色四份——stage 表 +0x14..+0x20 以
    // be7bc+be7b8 选份，0x424fd0:27）。缺失时降级为对话缺席（519 即过、
    // 事件契约行为不变）；浏览器侧资产清单补 st0N[a-d].msg 属接线 TODO。
    char msg_name[16];
    std::snprintf(msg_name, sizeof msg_name, "st%02d%c.msg", int(progress.stage),
                  'a' + (progress.character & 3));
    std::vector<u8> msg_bytes;
    if (host.read_asset(msg_name, msg_bytes, false)) {
        std::string msg_error;
        if (dialog_file.open(msg_bytes.data(), u32(msg_bytes.size()), msg_error)) {
            dialog_ready = true;
            battle.sim.dialog_host = this;  // ins_518/519 接口位
        } else {
            return fail("Stage msg invalid: " + std::string(msg_name) + ": " + msg_error);
        }
    }
    hud.prepare_stage_assets();
    if (!hud.initialize(HudEntryContext{})) return fail(hud.error);
    return true;
}

bool RunScene::update() {
    if (!error.empty()) return false;
    if (environment.paused) return true;  // 暂停覆盖层冻结对局（th15 environment.paused 同机制）
    // 对局内覆盖层三旗冻结（0x42c550:263：GameInf+0x60 的 0x10/0x20/0x40 任一
    // 非零 → 对局 update 返 3 不推帧；0x4000 通关演出不在门内照常推帧）。
    if (progress.scene_flags & 0x70u) return true;
    progress.stage_frame++;
    age.tick(&progress.rate);
    GameInput input;
    input.held = input_held;
    input.pressed = input_pressed;
    input.repeated = input_repeated;
    if (!battle.step(input)) return fail(battle.error());
    // 对话解释器（0x4265a0 每帧驱动：ECL 步进后同帧推进；Z 边沿 = bit0）。
    if (dialog.active()) dialog.step((input_pressed & 0x01u) != 0);
    // Boss 登场变暗推进：boss 槽存活（midboss 同走 512 setBoss）→ 30 帧淡入
    // 0.6 α；消失反向淡出。事件流实证：2381 MBoss spawn 与 setBoss 同帧。
    if (battle.sim.boss() != nullptr) {
        if (boss_dim < 30) boss_dim++;
    } else if (boss_dim > 0) {
        boss_dim--;
    }
    if (scenery && !scenery->update(animations, environment, progress.rate))
        return fail(scenery->error.empty() ? "Stage scenery update failed" : scenery->error);
    if (!presentation->sync(battle, input_held)) return fail(presentation->error());
    hud.update(HudFrameContext{}, host);
    hud.update_score();
    // 覆盖层/通关演出推进（0x43dbc0 行为子集 + GameInf 0x4000 路径）。
    post_flow.update();
    return hud.error.empty() || fail(hud.error);
}

void RunScene::open_dialog(i32 entry) {
    // 0x428820 else 分支：已有对话先销毁（+0x198 换手），再开新 entry。
    if (!dialog_ready) return;
    std::string dialog_error;
    if (!dialog.open(dialog_file, u32(entry), *this, dialog_error))
        fail("Dialog entry " + std::to_string(entry) + ": " + dialog_error);
}

void RunScene::msg_bgm(i32) {
    // BGM 族（opcode 2/19/20/31——stage 表 0x64/0x2c/难度门/0x68 曲名）：
    // 音频接线属 Application 层 TODO（本阶段对话乐句切换不生效）。
}

void RunScene::msg_portrait_load() {
    // opcode 1：按角色装载对话立绘 bank（0x4c0aa0 + be7b8*4）——视觉阶段
    // 随结算窗 oracle 一起锚定（TODO）。
}

void RunScene::msg_boss_battle() {
    // opcode 22/27 → 0x43af70（Boss 战设置）：battle 侧接线 TODO。
}

void RunScene::msg_stage_clear() {
    // opcode 21 → 0x42cd40 关终了（Boss 后对话 entry 尾的通关触发链入口）。
    post_flow.on_stage_clear();
}

bool RunScene::draw_stage(u32 pass) {
    if (!error.empty()) return false;
    if (!host.camera(DrawCamera::Fullscreen, false)) return fail("Stage camera unavailable");
    if (!presentation->draw(pass)) return fail(presentation->error());
    return true;
}

bool RunScene::draw_scenery(u32 pass) {
    // STD 舞台背景：pass 0 = 全屏黑清屏 + 雾色清屏 + 层 0-7 背景组；
    // pass 1 = 层 8-11 前景组。原版 3D 取景为全屏（golden 左边带随场景明暗
    // 变化实证：游戏区外左侧仍见 3D 背景，右侧/上下为 front.anm 框架覆盖）。
    // 两组均以恢复全屏 2D 相机/关雾收尾（后续 AnmDrawSchedule 层 0 起继承）。
    if (!error.empty()) return false;
    if (!scenery || !scenery->ready()) return true;
    auto& graphics = host.stage_graphics();
    auto& renderer = scene_renderer;
    const GraphicsViewport viewport{0, 0, environment.screen_width, environment.screen_height, 0, 1};
    if (pass == 0) {
        renderer.invalidate();
        graphics.pipeline().fogMode = touhou::graphics::FogMode::Linear;
        if (!graphics.clear_target(0xff000000, &viewport))
            return fail("Scenery screen clear failed");
        graphics.set_depth_test(true);
        graphics.set_depth_compare(touhou::graphics::Compare::LessEqual);
        graphics.set_depth_mask(true);
        const u32 fog = scenery->camera().fog.color;
        if (!graphics.clear_target(fog, &viewport))
            return fail("Scenery region clear failed");
    }
    if (!scenery->draw(animations, renderer, graphics, pass == 0 ? 0 : 8, pass == 0 ? 7 : 11,
                       viewport, {float(viewport.width), float(viewport.height)}))
        return fail(scenery->error.empty() ? "Stage scenery draw failed" : scenery->error);
    if (pass == 1) {
        renderer.invalidate();
        graphics.set_fog(false);
        graphics.set_depth_compare(touhou::graphics::Compare::Always);
        graphics.set_depth_mask(false);
        if (boss_dim > 0) {
            // Boss 变暗黑幕：仅游戏区（128,16)-(512,464)，浮点几何与游戏区
            // 边界一致；纯色 Strip 直交（无纹理，漫色一阶）。
            auto& state = graphics.pipeline();
            state.blend = true;
            state.sourceBlend = touhou::graphics::BlendFactor::SourceAlpha;
            state.destinationBlend = touhou::graphics::BlendFactor::InverseSourceAlpha;
            state.blendEquation = touhou::graphics::BlendEquation::Add;
            state.alphaTest = false;
            state.depthTest = false;
            state.depthWrite = false;
            state.textureTransform = false;
            state.color.operation = state.alpha.operation =
                touhou::graphics::ColorOperation::First;
            state.color.first = state.alpha.first = {touhou::graphics::ArgumentSource::Diffuse};
            graphics.set_layout(touhou::graphics::VertexLayout::ScreenColor);
            struct DimVertex {
                Vec3 position;
                float reciprocal_w;
                u32 color;
            };
            const float alpha =
                float(float(boss_dim) * (1.f / 30.f)) * 0.6f;  // 30 帧 → 0.6
            const u32 shade = u32(u8(i32(alpha * 255.f))) << 24;
            const DimVertex quad[] = {{{128, 16, 0}, 1, shade},
                                      {{512, 16, 0}, 1, shade},
                                      {{128, 464, 0}, 1, shade},
                                      {{512, 464, 0}, 1, shade}};
            graphics.primitives(touhou::graphics::Topology::Strip, 2, quad, sizeof(DimVertex));
        }
        if (!host.camera(DrawCamera::Fullscreen, false))
            return fail("Scenery camera restoration failed");
    }
    return true;
}

bool RunScene::draw() {
    if (!error.empty()) return false;
    hud.draw(HudDrawContext{}, host);
    // 对话文本预览（S3 第一阶段）：仅 ASCII 行经 hud_text 通道显示；全角
    // 假名/汉字需对话字体（原版 0x42a120 名牌/0x46e8f0 文本 VM 的逐字栅格），
    // 属渲染阶段 TODO——届时换全角通道并按 oracle draws 对齐。
    if (dialog.active()) {
        const auto& state = dialog.state();
        for (u32 row = 0; row < state.lines.size() && row < 2; row++) {
            const std::string& text = state.lines[row];
            bool representable = !text.empty();
            for (char ch : text)
                if ((u8(ch) & 0x80u) != 0) representable = false;
            if (!representable) continue;
            HudTextDraw preview;
            preview.position = {state.box_x, state.box_y + 18.f * float(row), 0};
            preview.style.color = 0xffffffff;
            preview.text = text;
            host.hud_text(preview);
        }
    }
    return hud.error.empty() || fail(hud.error);
}

}  // namespace th13
