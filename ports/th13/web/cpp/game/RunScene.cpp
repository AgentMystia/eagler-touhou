#include "RunScene.hpp"

#include <cstdio>

namespace th13 {

namespace {
// 场景回调优先级（th13 原版调度位待锚定；先按 th15 同族布局取值）：
// 更新 20（标题 6 与暂停 11 之间），HUD 文本 76（动画层 75 之后、字幕 78 之前）。
// 战斗直绘穿插 AnmDrawSchedule 各层：自机 23（00441e5a→00443df0）/
// 自机弹 20（原版 layer9；不能在 priority14 的背景合成之前绘制）/
// 敌机 19（层 8 同拍）/敌弹 43（层 20 之后，注册晚于动画层回调故同值仍靠后）。
// STD 舞台背景两组均在动画层 0（优先级 5）之前：背景组 3 / 前景组 4
// （th15 StageDrawFrame 3/6 的 th13 排位：层 0 前置约束下压缩到 3/4）。
constexpr i32 kRunUpdatePriority = 20;
constexpr i32 kRunTextPriority = 76;
constexpr std::array<i32, 4> kStageDrawPriorities = {20, 23, 19, 43};
constexpr std::array<i32, 2> kSceneryDrawPriorities = {3, 4};
}  // namespace

RunScene::RunScene(SessionState& p, HudPlayerState& player, HudScoreState& score,
                   AnmManager& manager, AnmRenderer& scene_view, AnmEnvironment& e,
                   RunSceneServices& h, FrameScheduler& f, StagePresentation::Banks banks,
                   i32 front, i32 ascii, i32 scenery_bank, const std::string& std_name,
                   GameRandom* shared_random)
    : progress(p),
      animations(manager),
      scene_renderer(scene_view),
      environment(e),
      host(h),
      scheduler(f),
      player_state(player),
      score_state(score),
      hud(animations, p, player, score, front, ascii),
      battle(shared_random),
      presentation(std::make_unique<StagePresentation>(animations, scene_renderer, banks,
                                                       kPlayerShotSprite)),
      msg_presentation(std::make_unique<MsgPresentation>(
          animations, MsgPresentation::Banks{banks.player, banks.stage})),
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
    if (msg_presentation) msg_presentation->release_all();  // 对话立绘/名牌 VM 归还
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
    // 关卡 ECL 名（stage 表 0x4bb4f0 行 +0x8；换关 0xc 后 Application 已按
    // DAT_004be81c 装载 st0N.ecl/std，此处取同表同款——非首关不再误装 st01）。
    char stage_ecl[16];
    std::snprintf(stage_ecl, sizeof stage_ecl, "st%02d.ecl", int(progress.stage));
    if (!battle.load(provider, stage_ecl, 1 << (progress.difficulty & 3), host, progress,
                     player_state, score_state, nullptr,
                     battle_sht.empty() ? nullptr : battle_sht.c_str()))
        return fail(battle.error().empty() ? "Stage battle load failed" : battle.error());
    // 敌机句柄化默认路径（2026-10-23 基座治理整改）：ECL 303/306 建池 VM
    // 入 registry（0x41fe80/case3 同构——oracle 极小环境路径），表现层手工
    // 敌机段跳过（enemies_from_ecl），敌机销毁回收由 retire_sink 钩承担。
    // 逃生开关：sim.ecl_slot_anm=false 回退双绘制旧路径。
    battle.sim.ecl_slot_anm = true;
    presentation->enemies_from_ecl = true;
    battle.sim.retire_sink = [this](u32 serial) {
        // 0x418e00 销毁支路的 ANM 面：回收该敌机全部槽位 VM
        //（0x470110 语义：retire 树 + 句柄归零）。敌机此刻仍在 sim.enemies
        //（erase 前调用），按 serial 查槽位句柄。
        for (const auto& enemy : battle.sim.enemies)
            if (enemy->serial == serial) {
                for (u32& handle : enemy->anm_slot_handle)
                    if (handle && !animations.retire(handle))
                        fail("Enemy retire animation: " + animations.error);
                return;
            }
    };
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
    // 对话立绘 bank/脚本绑定（stage 表 0x4bb4f0 行 + ECL ANIM 名单全局号换算）：
    // 只在 msg 资产存在时绑定；失败即场景错误（不静默降级成无立绘对话）。
    if (dialog_ready && !msg_presentation->bind_stage_scripts(
                            int(progress.stage), battle.sim.anim_list,
                            [this](const std::string& name, std::vector<u8>& bytes) {
                                return host.read_asset(name, bytes);
                            }))
        return fail("Dialog presentation bind: " + msg_presentation->error);
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
    // 0x448200:21,35 + 0x414f30：唯一 GameInput 实例（BrowserRuntime 采样、
    // 按游戏帧 cadence update 恰一次、Application::step 传入）——本场景直接
    // 消费该实例的 held/pressed/released/repeated/long_held，不自持第二实例
    //（task 卡 stage1-replay-runtime 禁止 P3；原版 0x4e49f0 全局单对象同构）。
    // 跨场景 previous 连续性由实例生命周期自然携带（BrowserRuntime 全局），
    // 菜单持键进关的首帧沿与原版一致。暂停/覆盖层冻结帧门内本函数早退不消费；
    // 宿主照常逐帧推进实例，门后恢复消费当前帧值（原版 00414f30 无条件逐帧
    // 语义由宿主侧承接）。测试直调 update() 未先 controls() 时消费零输入。
    static const GameInput kNoInput{};
    const GameInput& input = input_frame ? *input_frame : kNoInput;
    if (!battle.step(input)) return fail(battle.error());
    // 对话解释器（0x4265a0 每帧驱动：ECL 步进后同帧推进；Z 边沿 = bit0）。
    // 推进键面（0x428ae0:122-129）：Z 持续位 0x1、Ctrl 0x200（快进）、Enter
    // 0x80000 与 opcode 11 放行共用；持续帧数 = GameInput.duration（0x42b3f0
    // 的 0x4e49f0+0x194+key*4 同构）。
    if (dialog.active()) {
        MsgAdvance advance;
        advance.z_pressed = (input.pressed & 0x01u) != 0;
        advance.held = input.held;
        advance.z_frames = input.duration[0];
        advance.ctrl_frames = input.duration[9];
        dialog.step(advance);
    }
    // Boss 登场变暗推进：boss 槽存活（midboss 同走 512 setBoss）→ 30 帧淡入
    // 0.6 α；消失反向淡出。事件流实证：2381 MBoss spawn 与 setBoss 同帧。
    if (battle.sim.boss() != nullptr) {
        if (boss_dim < 30) boss_dim++;
    } else if (boss_dim > 0) {
        boss_dim--;
    }
    if (scenery && !scenery->update(animations, environment, progress.rate))
        return fail(scenery->error.empty() ? "Stage scenery update failed" : scenery->error);
    if (!presentation->sync(battle, input.held)) return fail(presentation->error());
    // Boss HUD 每帧视图装配（HudFrameContext.boss；0x4265a0 驱动面，RunScene
    // 装配契约）：可证字段接实值，未锚定缺口保持默认并登记，不伪造。
    BossFrameInput boss_view;
    boss_view.boss = battle.sim.boss();           // mgr+0x5c 槽 0
    boss_view.dialog_active = dialog.active();    // ctx+0x198 对话进行中
    boss_view.player_x = battle.player_x;         // DAT_004c22c4+0x5b8 世界位
    boss_view.player_y = battle.player_y;         // +0x5bc
    boss_view.boss_life =
        boss_view.boss ? i32(boss_view.boss->life) : 0;  // +0x5138 int 口径
    boss_view.trance_mode = trance_active(progress);     // DAT_004c2178+0x7c bit1
    // 【缺口登记，非功能完成】boss_leaving（mgr+0x7c bit0）；timer_seconds
    // 符卡时长（-1 = 无计时，锚定前不倒计时）；life_initial（<=0 环隐藏，
    // 锚定前 boss 环不显示）；markers ratio（ECL 540/527）。
    HudFrameContext frame;
    frame.dialogue = boss_view.dialog_active;
    frame.boss = boss_view.boss ? &boss_view : nullptr;
    hud.update(frame, host);
    hud.update_score();
    // 覆盖层/通关演出推进（0x43dbc0 行为子集 + GameInf 0x4000 路径）。
    post_flow.update();
    return hud.error.empty() || fail(hud.error);
}

void RunScene::open_dialog(i32 entry) {
    // 0x428820 else 分支：已有对话先销毁（+0x198 换手），再开新 entry。
    if (!dialog_ready) return;
    if (msg_presentation) msg_presentation->release_all();
    std::string dialog_error;
    if (!dialog.open(dialog_file, u32(entry), *this, dialog_error))
        fail("Dialog entry " + std::to_string(entry) + ": " + dialog_error);
}

void RunScene::msg_bgm(i32 opcode) {
    // 0x428ae0 jump table（0x429c7c）逐 handler 实证分派——旧「BGM 族」注释
    // 仅对 19 成立（p58-dialogue 纠正）：
    //   2（0x429335）右立绘：entry<2 或无 Boss 对 = 中Boss 对（stage 表
    //     +0x6c/+0x70），否则 Boss 对（+0x64/+0x68）；
    //   19（0x4299df → 0x43ae40(1, stage+0x2c)）Boss 曲：播关首预载槽 1
    //     （0x42b990:215 载入；Application.begin_run 侧预载）；
    //   20（0x429a94）名牌 VM：stage 表 +0x54/+0x58 缺席用 +0x5c/+0x60；
    //   31（0x42948c）右立绘 Boss 对无条件。
    if (!msg_presentation) return;
    switch (opcode) {
    case 2:
        if (!msg_presentation->side_portrait(dialog.state().entry))
            fail("Dialog side portrait: " + msg_presentation->error);
        break;
    case 19:
        if (!host.queue_music(2, 1, "")) fail("Boss music queue failed");
        break;
    case 20:
        if (!msg_presentation->nameplate())
            fail("Dialog nameplate: " + msg_presentation->error);
        break;
    case 31:
        if (!msg_presentation->boss_side_portrait())
            fail("Dialog boss portrait: " + msg_presentation->error);
        break;
    default:
        break;
    }
}

void RunScene::msg_portrait_load() {
    // opcode 1（0x429285）：自机立绘根 VM——玩家 plXX bank 脚本表
    // 0x4c0aa0[be7b8]（EXE 实读 = 29,34,30,28）。
    if (msg_presentation && !msg_presentation->player_portrait(progress.character & 3))
        fail("Dialog player portrait: " + msg_presentation->error);
}

void RunScene::msg_boss_battle() {
    // opcode 22/27 → 0x43af70（Boss 战设置）：battle 侧接线 TODO（Boss 域）。
}

void RunScene::msg_stage_clear() {
    // opcode 21 → 0x42cd40 关终了（Boss 后对话 entry 尾的通关触发链入口）。
    post_flow.on_stage_clear();
}

// ——追加可视化通知钩子（0x428ae0 逐 handler 证据；见 MsgPresentation.hpp）——

void RunScene::msg_portrait_interrupt(i32 opcode, i32 label) {
    // 13/14（0x4298d4/0x4298e8）：label = arg+0x11 的 0x46fca0 立即中断——
    // 表情切换；13 打左（自机立绘）、14 打右（Boss 立绘）。
    if (!msg_presentation) return;
    const bool ok = opcode == 0x0d ? msg_presentation->interrupt_player(label)
                                   : msg_presentation->interrupt_side(label);
    if (!ok) fail("Dialog portrait interrupt: " + msg_presentation->error);
}

void RunScene::msg_speaker_switch(i32 opcode) {
    // 7（0x429641）：左 label2 亮、右 label3 暗；8（0x4296ee）：左 3、右 2。
    if (!msg_presentation) return;
    const bool ok = opcode == 0x08 ? msg_presentation->interrupt_player(3) &&
                                         msg_presentation->interrupt_side(2)
                                   : msg_presentation->interrupt_side(3) &&
                                         msg_presentation->interrupt_player(2);
    if (!ok) fail("Dialog speaker switch: " + msg_presentation->error);
}

void RunScene::msg_release(i32 opcode) {
    // 4/23（0x4295cf/0x4295a9）左槽；5/24（0x4295e9/0x4295bc）右槽含名牌；
    // 6/18（0x42960c/0x429250）名牌 + 文本区（port 单名牌槽 + dialog 行块）。
    if (!msg_presentation) return;
    switch (opcode) {
    case 0x04:
    case 0x17:
        msg_presentation->release_player();
        break;
    case 0x05:
    case 0x18:
        msg_presentation->release_side();
        break;
    case 0x06:
    case 0x12:
        msg_presentation->release_nameplates();
        break;
    default:
        break;
    }
    if (!msg_presentation->error.empty())
        fail("Dialog release: " + msg_presentation->error);
}

void RunScene::msg_music_fade(i32 opcode) {
    // 0x43af70 曲命令 5 淡出：opcode 22 常数 2.0s（0x4aed90 实读；关 6 分支
    // 0x4aea74 = 8.0s）、27 取指令 arg 浮点（st0N[a-d] 未用 27，arg 未透传
    // 该钩子——登记 HANDOFF）。
    (void)opcode;
    if (!host.queue_music(5, progress.stage == 6 ? 8 : 2, ""))
        fail("Dialog music fade queue failed");
}

void RunScene::msg_advance_blip() {
    // opcode 11 计声放行（0x460bc0(ctx,0)）：原版 voice 0 提示音。
    if (!host.hud_sound(0)) fail("Dialog advance sound failed");
}

void RunScene::msg_dialog_end() {
    // entry 终止（0x4265a0 当帧销毁）：全部对话 VM 释放（原版对话对象销毁
    // 连带 +0x40/+0x44/+0x5c 槽位回收）。
    if (msg_presentation) {
        msg_presentation->release_all();
        if (!msg_presentation->error.empty())
            fail("Dialog end release: " + msg_presentation->error);
    }
}

bool RunScene::draw_stage(u32 pass) {
    if (!error.empty()) return false;
    // Original game sprites are centered in the offscreen @R surface. The
    // supervisor's text84 pass later samples128..512 to screen32..416.
    // Do not translate either these vertices or their full-screen camera.
    if (!host.camera(DrawCamera::Fullscreen, false)) return fail("Stage camera unavailable");
    if (!presentation->draw(pass)) return fail(presentation->error());
    return true;
}

bool RunScene::draw_scenery(u32 pass) {
    // STD 背景两组先画入640x480的原版中间 @R，而不是最终游戏框的
    // 384x448 viewport。PlayfieldTargets/text84只把中央128..512合成到
    // 屏幕32..416；不能从中间背景的边带推断最终framebuffer画面。
    // 两组均以恢复全屏 2D 相机/关雾收尾（后续 AnmDrawSchedule 层 0 起继承）。
    if (!error.empty()) return false;
    if (!scenery || !scenery->ready()) return true;
    auto& graphics = host.stage_graphics();
    auto& renderer = scene_renderer;
    // 0x4dcaf4 STD 相机视口（43bbc0: 4dcbcc..4dcbd8 = {116,2,408,476}）：
    // 原版 STD 取景按 408x476（aspect≈0.857），非全屏 4:3。黑/雾清屏与 STD
    // 绘制同用该视口（r4/render/integration.md 第二批 patch A）。
    const GraphicsViewport viewport{116, 2, 408, 476, 0, 1};
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
            // Boss 黑幕仍在中间 @R 的128..512域；与后续text84相同的
            // 源裁剪区，最终落到32..416。这里改32反而会少覆盖右侧。
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
