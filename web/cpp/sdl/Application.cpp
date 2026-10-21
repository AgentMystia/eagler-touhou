// Application 主流程（th15 Application.cpp 裁剪：destination 驱动、装载挡板、
// 帧序固定「装载完成 → 场景 controls+scheduler → 显示绘制 → 装载挡板 →
// apply_destination」，docs/plans/ui-base.md §四）。
#include "ApplicationState.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace th13::sdl {

namespace {
// 常驻 ANM bank 表（th13 与 th15 同名同布局，dumps/anm-list.txt 实证）。
constexpr std::pair<const char*, i32> kBanks[7] = {
    {"text.anm", 0}, {"sig.anm", 1}, {"ascii.anm", 2},  {"effect.anm", 8},
    {"title.anm", 16}, {"title_v.anm", 17}, {"help.anm", 19},
};
// front 面板在 begin_run 时由场景资产登记（残机/灵击脚本 30–45 已验证）。
constexpr i32 kFrontBank = 3;
// Stage1 战斗资产 bank：stage = ANIM 名单顺序合并（脚本号全局连续，ECL 302/303
// 引用即此序）、bullet = 弹幕外观表脚本、player = plXX.anm（选人定档）、
// scenery = STD 引用 ANM（st01.std → st01wl.anm）。
constexpr i32 kStageBank = 4, kBulletBank = 5, kPlayerBank = 6, kSceneryBank = 7;
}  // namespace

Application::Application() : state(std::make_unique<ApplicationState>()) {}
Application::~Application() = default;
bool Application::initialize(bool device) { return state->initialize(device); }
bool Application::step(u32 held, u32 pressed, u32 repeated, float fps, bool focus) {
    return state->step(held, pressed, repeated, fps, focus);
}
const std::string& Application::error() const { return state->failure; }
const i32* Application::projection() const { return state->projection(); }

bool ApplicationState::fail(const std::string& why) {
    if (failure.empty()) failure = why.empty() ? "Application operation failed" : why;
    return false;
}

bool ApplicationState::read(const std::string& name, std::vector<u8>& bytes) {
    const auto at = name.find_last_of("/\\");
    const auto base = name.substr(at == std::string::npos ? 0 : at + 1);
    if (base.empty() || base == "." || base == "..") return fail("Invalid game asset name");
    const auto found = resource_cache.find(base);
    if (found != resource_cache.end()) {
        bytes = found->second;
        return true;
    }
    size_t size = 0;
    auto* data = static_cast<u8*>(SDL_LoadFile(("/assets/" + base).c_str(), &size));
    if (!data) return fail("Missing game asset: " + base);
    bytes.assign(data, data + size);
    SDL_free(data);
    if (bytes.size() <= 1024 * 1024 && base.size() >= 4 && base.substr(base.size() - 4) != ".wav" &&
        base.substr(base.size() - 4) != ".anm")
        resource_cache.emplace(base, bytes);
    return true;
}

bool ApplicationState::prepare_platform() {
    if (platform_prepared) return true;
    if (!graphics.initialize()) return fail(graphics.error);
    platform_prepared = true;
    return true;
}

bool ApplicationState::prepare_loading() {
    if (!prepare_platform()) return false;
    std::vector<u8> bytes;
    loading_environment.resolution_scale = 1;
    loading_environment.raster_scale = float(graphics.render_scale);
    constexpr std::pair<const char*, i32> loading_banks[2] = {{"sig.anm", 1}, {"ascii.anm", 2}};
    for (const auto& bank : loading_banks) {
        if (!loading_animations.resource(bank.second) &&
            (!read(bank.first, bytes) ||
             !loading_animations.load(bank.second, bytes.data(), u32(bytes.size())) ||
             !graphics.preload(*loading_animations.resource(bank.second))))
            return fail("Loading artwork: " + loading_animations.error + graphics.error);
    }
    return draw_loading(true);
}

bool ApplicationState::loading_waiting() const { return loading_startup || pending_load != 0; }

bool ApplicationState::draw_loading(bool signature) {
    graphics.presentation.reset();
    if (!loading_animations.retire(loading_signature) ||
        !loading_animations.retire(loading_prayer))
        return fail(loading_animations.error);
    if (signature) {
        loading_signature = loading_animations.create(1, 0);
        if (!loading_signature) return fail(loading_animations.error);
    }
    loading_prayer = loading_animations.create(2, 17, -1, 0, {960, 784, 0});
    if (!loading_prayer) return fail(loading_animations.error);
    loading_startup = signature;
    loading_frames = 0;
    loading_debt = 0;
    return render_loading();
}

bool ApplicationState::render_loading() {
    renderer.invalidate();
    graphics.backend.pipeline() = touhou::graphics::PipelineState{};
    if (!graphics.select_target(nullptr, 0) || !graphics.clear_target(0xff000000, nullptr))
        return fail(graphics.error);
    ScreenViews loading_views{renderer, loading_environment};
    if (!loading_views.camera(DrawCamera::Fullscreen, false)) return fail("Loading camera unavailable");
    const auto* credit = loading_animations.registry.find(loading_signature);
    for (u32 layer = 0; layer < 33; ++layer) {  // th13 层域：主链 0-30 + 副链 31/32
        if (!renderer.draw_layer(loading_animations.registry.layer(layer))) return fail(renderer.error);
        if (credit && layer == u32(credit->visual.layer)) {
            renderer.flush();
            graphics.draw_startup_branding(credit->visual.color);
        }
    }
    renderer.flush();
    graphics.present();
    return true;
}

bool ApplicationState::advance_loading(double delta) {
    // 启动保留 180 帧原生装载动画；仅推进隔离的装载 ANM 时钟（th15 同款）。
    loading_debt += std::clamp(delta, 0., .1);
    while (loading_debt + 1e-9 >= 1. / 60. && loading_frames < 180) {
        loading_debt = std::max(0., loading_debt - 1. / 60.);
        if (!loading_animations.update(false) || !loading_animations.update(true))
            return fail(loading_animations.error);
        ++loading_frames;
    }
    if (!render_loading()) return false;
    if (loading_frames >= 180) {
        loading_startup = false;
        loading_debt = 0;
    }
    return true;
}

bool ApplicationState::initialize(bool device) {
    if (initialized) return fail("Application already initialized");
    if (!prepare_platform()) return false;
    if (!loading_animations.resource(2) && !prepare_loading()) return false;
    if (!fonts.initialize() || !audio_device.initialize(device)) return fail(fonts.error);
    environment.raster_scale = float(graphics.render_scale);
    environment.render_target_has_alpha = true;
    environment.screen_offsets = {320, 16, 320, 16};
    environment.playfield_origin = {128, 16};
    if (!files.initialize()) return fail(files.error);
    animations.resource_release = [this](const AnmResource& resource) { graphics.unload(resource); };
    std::vector<u8> bytes;
    for (const auto& bank : kBanks) {
        if (!read(bank.first, bytes) ||
            !animations.load(bank.second, bytes.data(), u32(bytes.size())))
            return fail("Application ANM load: " + std::string(bank.first) + ": " + animations.error);
        animations.name_resource(bank.second, bank.first);
        if (bank.second == 2) environment.fallback_sprite_resource = animations.resource(2);
        if (!graphics.preload(*animations.resource(bank.second))) return fail(graphics.error);
    }
    display = std::make_unique<SceneDisplay>(scheduler, animations, environment, graphics, renderer,
                                             views, captions, 2);
    if (!display->initialize()) return fail(display->error);
    for (u32 index = 0; index < animation_updates.size(); index++) {
        auto& callback = animation_updates[index];
        callback.owner = this;
        callback.enabled = true;
        callback.run = index == 0 ? [](void* p) -> i32 {
                                 return static_cast<ApplicationState*>(p)->animations.update(true) ? 1 : 5;
                             }
                                  : [](void* p) -> i32 {
                                 return static_cast<ApplicationState*>(p)->animations.update(false) ? 1 : 5;
                             };
        if (scheduler.add(callback, FramePass::Update, index == 0 ? 9 : 34) < 0)
            return fail("Application animation callback registration failed");
    }
    initialized = true;
    return begin_title(0);
}

ApplicationState::~ApplicationState() {
    title.reset();
    release_run();
    for (auto& callback : animation_updates) scheduler.remove(callback);
    display.reset();
    animations.resource_release = nullptr;
}

void ApplicationState::release_run() {
    graphics.presentation.reset();
    pause.reset();
    run.reset();  // ~RunScene 先释放战斗 ANM 子树（外部 VM 归还 registry）
    // 对局 bank 卸载（unload 从 banks 摘除并落 retired 队列，供重开再装载）；
    // 常驻 bank（text/ascii/effect）只 retire 活动 VM，资源本体跨场景常驻。
    for (const auto bank : {kFrontBank, kStageBank, kBulletBank, kPlayerBank, kSceneryBank})
        animations.unload(bank);
    for (const auto bank : {0, 2, 8}) animations.retire_resource(bank);
    animations.collect_resources();
    for (auto& callback : animation_updates) callback.enabled = true;
    environment.paused = false;
    environment.screen_translation = {};
    environment.background_delta = {};
    environment.screen_offsets = {320, 16, 320, 16};
    environment.playfield_origin = {128, 16};
    views.configure(environment);
    renderer.invalidate();
    graphics.backend.pipeline() = touhou::graphics::PipelineState{};
    if (display) display->detach();
}

bool ApplicationState::preload_scenery_animation(i32 bank, const std::string& name) {
    // STD 引用 ANM 装载（begin_run 场景期专用；release_run 随对局 bank 卸载）。
    std::vector<u8> bytes;
    if (animations.resource(bank)) return true;
    if (!read(name, bytes) || !animations.load(bank, bytes.data(), u32(bytes.size())))
        return fail("Scenery ANM load: " + name + ": " + animations.error);
    animations.name_resource(bank, name);
    if (!graphics.preload(*animations.resource(bank)))
        return fail("Scenery ANM preload: " + name + ": " + graphics.error);
    return true;
}

bool ApplicationState::begin_title(i32 reason) {    title.reset();
    release_run();
    current_destination = i32(Destination::Title);
    title = std::make_unique<TitleScene>(progress, selection, animations, *this, scheduler,
                                         TitleSceneResources{});
    title->state.return_reason = reason;
    return title->error.empty() || fail(title->error);
}

bool ApplicationState::begin_run() {
    title.reset();
    release_run();
    for (auto& callback : animation_updates) callback.enabled = false;
    progress.stage = selected_stage;
    progress.starting_stage = selected_stage;
    progress.stage_frame = 0;  // 重开归零（原版 supervisor 重建对局状态）
    progress.scene_flags = 0;
    progress.rate = 1;
    progress.new_run = true;
    replay_active = false;  // begin_run 重建对局；回放会话由 begin_replay_run 再接管
    std::vector<u8> bytes;
    if (!animations.resource(kFrontBank) &&
        (!read("front.anm", bytes) ||
         !animations.load(kFrontBank, bytes.data(), u32(bytes.size())) ||
         !graphics.preload(*animations.resource(kFrontBank))))
        return fail("Run front asset: " + animations.error + graphics.error);
    // Stage1 场景资产（Stage2+ 分期）：ANIM 名单顺序合并成单一 bank——
    // ECL 302/303/306 的脚本号即该合并表的全局序号（0x46e0b0/0x46e230 语义）。
    const char* stage_ecl = selected_stage == 1 ? "st01.ecl" : "st01.ecl";
    std::vector<u8> ecl;
    if (!read(stage_ecl, ecl)) return fail(std::string("Stage ECL missing: ") + stage_ecl);
    EclResource head;
    if (!head.open(ecl.data(), u32(ecl.size())) || head.animations.empty())
        return fail("Stage ECL ANIM list unavailable");
    AnmResource stage;
    for (const auto& name : head.animations) {
        if (!read(name, bytes)) return fail("Stage ANM missing: " + name);
        if (!stage.append(bytes.data(), u32(bytes.size())))
            return fail("Stage ANM invalid: " + name);
    }
    if (!animations.adopt(kStageBank, std::move(stage)) ||
        !graphics.preload(*animations.resource(kStageBank)))
        return fail("Stage ANM bank: " + animations.error + graphics.error);
    if (!read("bullet.anm", bytes) ||
        !animations.load(kBulletBank, bytes.data(), u32(bytes.size())) ||
        !graphics.preload(*animations.resource(kBulletBank)))
        return fail("Bullet ANM bank: " + animations.error + graphics.error);
    const char* player_files[4] = {"pl00.anm", "pl01.anm", "pl02.anm", "pl03.anm"};
    const i32 character = progress.character & 3;
    if (!read(player_files[character], bytes) ||
        !animations.load(kPlayerBank, bytes.data(), u32(bytes.size())) ||
        !graphics.preload(*animations.resource(kPlayerBank)))
        return fail("Player ANM bank: " + animations.error + graphics.error);
    StagePresentation::Banks banks{kStageBank, kBulletBank, kPlayerBank};
    // STD 舞台背景资产名（Stage2+ 分期；Stage1 = st01.std → st01wl.anm）。
    const char* stage_std = selected_stage == 1 ? "st01.std" : "st01.std";
    run = std::make_unique<RunScene>(progress, selection_player, selection_score, animations,
                                     renderer, environment, *this, scheduler, banks, kFrontBank,
                                     2, kSceneryBank, stage_std);
    if (!replay_sht.empty()) run->battle_sht = replay_sht;  // 回放会话移速源（§7.2）
    if (!run->initialize()) return fail(run->error);
    pause = std::make_unique<RunPause>(progress, animations, environment, *this, scheduler, kFrontBank);
    if (!pause->prepare()) return fail(pause->error);
    return true;
}

bool ApplicationState::begin_replay_run(const u8* data, size_t size, u32 stage_index) {
    // 回放会话（回放侧 0x448200 的对局直入版）：标题流程全部跳过，难度/角色/
    // 关卡按录像头配置；命数给足——近似判定下的死亡按无敌复活继续（harness
    // replay 同款观察口径），终局菜单不触发。
    if (!replay_file.decode(data, size)) return fail("Replay decode: " + replay_file.error());
    if (stage_index >= replay_file.stages.size())
        return fail("Replay stage index out of range");
    const ReplayStage& stage = replay_file.stages[stage_index];
    replay_cursor = std::make_unique<ReplayInputCursor>(stage);
    replay_consumed = 0;
    selected_stage = stage.number > 0 ? stage.number : 1;
    progress.difficulty = replay_file.rank & 3;
    progress.character = replay_file.character & 3;
    selection_player = HudPlayerState{};  // 命数底板 8（step 内回填，HUD 域内不终局）
    selection_score = HudScoreState{};
    char sht[16];
    std::snprintf(sht, sizeof sht, "pl%02d%c.sht", replay_file.character & 3,
                  replay_file.shot_type == 0 ? 'a' : 'b');
    replay_sht = sht;
    // 直入对局：装载挡板/标题态就地清除（debug 泵不再经过 loading_waiting 分支）。
    loading_startup = false;
    loading_debt = 0;
    pending_load = 0;
    pending_destination = -1;
    if (!begin_run()) return false;
    replay_active = true;
    current_destination = i32(Destination::Run);
    return true;
}

u32 ApplicationState::replay_take_input() {
    // 输入流帧号即游戏帧（golden README「时间轴对齐」）：每游戏帧恰取一次；
    // 越界/EOF 后零态（0x448200:14-19 同源）。
    if (!replay_active || !replay_cursor || !replay_cursor->valid()) return 0;
    const ReplayFrame frame = replay_cursor->next();
    ++replay_consumed;
    return frame.state;
}

bool ApplicationState::apply_destination() {
    if (pending_destination < 0) return true;
    const auto next = pending_destination;
    pending_destination = -1;
    if (next == i32(Destination::Run)) {
        current_destination = next;
        return begin_run();
    }
    if (next == i32(Destination::Title) || next == i32(Destination::TitleFromGameOver) ||
        next == i32(Destination::TitleFromReplay)) {
        return begin_title(next == i32(Destination::TitleFromGameOver) ? 3 : 1);
    }
    if (next == i32(Destination::Quit)) {
        exiting = true;
        return true;
    }
    return fail("Unrecognized application destination: " + std::to_string(next));
}

bool ApplicationState::complete_loading() {
    if (pending_load) {
        const unsigned task = pending_load;
        pending_load = 0;
        if (task == 1) {
            if (!apply_destination()) return false;
        }
    }
    return true;
}

bool ApplicationState::step(u32 held, u32 pressed, u32 repeated, float fps, bool lost_focus) {
    (void)fps;
    if (!initialized || exiting || !failure.empty()) return false;
    if (!complete_loading()) return false;
    if (replay_active && run && selection_player.extra_lives < 8)
        selection_player.extra_lives = 8;  // 回放观察底板：近似判定的死亡不终局（HUD 域内）
    if (title) {
        title->controls({held, pressed, repeated, 0});
        if (scheduler.update() < 0)
            return fail(title->error.empty() ? title->frame.error.empty() ? animations.error
                                                                           : title->frame.error
                                                : title->error);
    } else if (run) {
        if (pause) {
            if (pause->active()) {
                pause->controls({pressed, repeated, lost_focus});
            } else if (pressed & 0x100) {  // Esc：暂停覆盖层
                if (!pause->open(PauseEntrance::Pause)) return fail(pause->error);
            } else if (lost_focus) {
                if (!pause->open(PauseEntrance::Pause)) return fail(pause->error);
            }
        }
        run->controls(held, pressed, repeated);  // 战斗输入（Z 射击/X 炸弹/方向）
        if (scheduler.update() < 0)
            return fail(run->error.empty() ? animations.error : run->error);
    } else {
        return fail("Application has no active scene");
    }
    frames++;
    if (!display->draw()) return fail(display->error);
    if (pending_destination == i32(Destination::Run) || pending_destination == i32(Destination::Ending)) {
        if (!draw_loading(false)) return false;
        pending_load = 1;
        return true;
    }
    return apply_destination() && (graphics.error.empty() || fail(graphics.error));
}

const i32* ApplicationState::projection() {
    projected.fill(0);
    projected[0] = title ? 0 : run ? 1 : -1;
    projected[1] = current_destination;
    projected[2] = progress.stage;
    projected[3] = progress.difficulty;
    projected[4] = progress.character;
    projected[5] = progress.stage_frame;
    projected[6] = i32(progress.scene_flags);
    projected[7] = i32(frames);
    if (title) {
        projected[8] = i32(title->state.screen);
        projected[9] = title->state.substate;
        projected[10] = title->state.menu.cursor;
        projected[11] = title->state.menu.count;
        projected[12] = i32(selection_player.mode_flags);
    } else if (run) {
        projected[8] = selection_score.score;
        projected[9] = selection_player.extra_lives;
        projected[10] = selection_player.bombs;
        projected[11] = selection_player.power;
        projected[12] = i32(selection_player.mode_flags);
        projected[13] = selection_player.deaths;
        projected[14] = pause ? i32(pause->state.screen) : 0;
    }
    return projected.data();
}

}  // namespace th13::sdl
