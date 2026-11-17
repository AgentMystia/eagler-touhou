#include "GameSession.hpp"

#include <vector>

namespace th16 {

bool GameSession::open(const u8* bytes, u32 size) {
    if (!archive_.open(bytes, size)) { failure = archive_.error; return false; }
    return true;
}

AnmResource* GameSession::resource(const char* name) {
    for (size_t i = 0; i < resource_names_.size(); ++i)
        if (resource_names_[i] == name) return &resources_[i];
    return nullptr;
}

bool GameSession::load_anm(const char* name, AnmResource*& out) {
    if (AnmResource* existing = resource(name)) { out = existing; return true; }
    std::vector<u8> bytes;
    if (!archive_.read(name, bytes)) { failure = archive_.error; return false; }
    resources_.reserve(16);  // 地址稳定：AnmManager 持有 resource 指针
    if (resources_.size() >= 16) { failure = "resource table full"; return false; }
    resources_.emplace_back();
    resource_names_.emplace_back(name);
    AnmResource& res = resources_.back();
    if (!res.parse(bytes.data(), u32(bytes.size()), name)) { failure = std::string(name) + ": " + res.error; return false; }
    for (u32 i = 0; i < res.entries.size(); ++i) {
        const auto& e = res.entries[i];
        if (e.has_data || e.name.empty() || e.name[0] == '@') continue;
        std::vector<u8> external;
        if (!archive_.read(e.name.c_str(), external)) { failure = archive_.error; return false; }
        if (!res.attach_external_texture(i, external.data(), u32(external.size()))) { failure = res.error; return false; }
    }
    if (on_loaded && !on_loaded(res)) { if (failure.empty()) failure = "texture upload failed"; return false; }
    out = &res;
    return true;
}

bool GameSession::begin_boot() {
    AnmResource* ascii = nullptr;
    AnmResource* sig = nullptr;
    // 0x407cb0 AsciiManager：ascii.anm → 槽 2
    if (!load_anm("ascii.anm", ascii)) return false;
    animations.load(AnmManager::kAsciiSlot, *ascii);
    // 0x43adc0：sig.anm → 槽 1
    if (!load_anm("sig.anm", sig)) return false;
    if (animations.load(1, *sig) == nullptr) { failure = "sig.anm load failed"; return false; }
    // 0x43adc0 余下同步装载：text.anm → 槽 0（DAT_004c17f8）；0x43ad20：front.anm → 槽 5；
    // 0x418790（EffectManager）：effect.anm → 槽 8、bullet.anm → 槽 7
    static constexpr struct { const char* name; int slot; } kBootBanks[] = {
        {"text.anm", 0}, {"front.anm", 5}, {"effect.anm", 8}, {"bullet.anm", 7},
    };
    for (const auto& bank : kBootBanks) {
        AnmResource* res = nullptr;
        if (!load_anm(bank.name, res)) return false;
        if (animations.load(bank.slot, *res) == nullptr) { failure = std::string(bank.name) + " load failed"; return false; }
    }

    supervisor.boot_frames = 0;
    supervisor.current = SceneId::Boot;

    // 0x43af60：启动幕绘制链（优先级 0x44）；0x43adc0 末尾启用
    boot_draw_chain_ = chains.create([this]() -> ChainResult {
        // 0x43b300：首帧 spawn script0（layer -1），之后 +0x638 递增
        if (supervisor.boot_frames == 0) {
            if (AnmFile* f = animations.file(1)) animations.spawn(*f, 0, -1);
        }
        ++supervisor.boot_frames;
        return ChainResult::Continue;
    }, 0x44, false);
    chains.enable(boot_draw_chain_);

    // 0x43b290：装载完成请求 Title
    supervisor.boot_init_finished();
    return true;
}

bool GameSession::enter_title_assets() {
    AnmResource* title = nullptr;
    AnmResource* title_v = nullptr;
    // 0x44ac70：title.anm → 槽 0x10，title_v.anm → 槽 0x11
    if (!load_anm("title.anm", title)) return false;
    if (!load_anm("title_v.anm", title_v)) return false;
    if (animations.load(0x10, *title) == nullptr) { failure = "title.anm load failed"; return false; }
    if (animations.load(0x11, *title_v) == nullptr) { failure = "title_v.anm load failed"; return false; }
    supervisor.title_assets_ready = true;
    supervisor.title_menu_live = false;
    return true;
}

bool GameSession::activate_title_menu() {
    // 0x44ac21：卸 sig.anm（槽 1），摘启动幕绘制链
    animations.kill_file(1);
    animations.tick_world();
    animations.unload(1);
    if (boot_draw_chain_) { chains.remove(boot_draw_chain_); boot_draw_chain_ = nullptr; }

    AnmFile* title_file = animations.file(0x10);
    AnmFile* title_v_file = animations.file(0x11);
    if (title_file == nullptr || title_v_file == nullptr) { failure = "title banks missing"; return false; }

    // 0x44af80 case 0 → 0x44a560(1) → 0x44b5f0：标题对象在首次更新链调用时自行生成装饰/logo
    title.activate(title_file, title_v_file, animations.file(AnmManager::kAsciiSlot), globals);
    globals.scene_request = -1;

    // 0x44ac54：启用标题更新链（优先级 6 → 0x44af80）
    title_update_chain_ = chains.create([this]() -> ChainResult {
        title.update(animations, supervisor.input);
        if (globals.scene_request >= 0) {
            supervisor.request(SceneId(globals.scene_request));
            globals.scene_request = -1;
        }
        return ChainResult::Continue;
    }, 6, true);
    chains.enable(title_update_chain_);

    supervisor.title_menu_live = true;
    return true;
}

void GameSession::sample_input() {
    supervisor.input.held = host_input | injected_input;
    supervisor.input.commit();
}

// 0x44af50 析构 / 0x43ce10 切换：摘标题更新链，标记 title*.anm VM 删除（0x46f270），释放槽
void GameSession::leave_title() {
    if (title_update_chain_) { chains.remove(title_update_chain_); title_update_chain_ = nullptr; }
    title_events.insert(title_events.end(), title.events.begin(), title.events.end());
    // 0x44ad20：析构时对落叶发射器（ascii.anm，+0x73c）interrupt 1，发射器与其子落叶在脚本 case 1 后自删
    animations.interrupt(title.vm[TitleScene::kLeafEmitter], 1);
    title = TitleScene{};
    animations.kill_file(0x10);
    animations.kill_file(0x11);
    animations.tick_world();
    animations.unload(0x10);
    animations.unload(0x11);
    supervisor.title_menu_live = false;
    supervisor.title_assets_ready = false;
}

bool GameSession::enter_game() {
    // 0x42d700：flags |= 4、模式参数；0x42cb60 装载线程（此处同步）
    game = GameManager{};
    game.flags = GameManager::kLoading;
    game.mode = 0;
    game.entry = &stage_entry(globals.stage);

    // 0x409db0 / 0x4097c0 / 0x40ac30：STD → 背景 ANM 进槽 3 + (stage & 1)
    std::vector<u8> std_bytes;
    if (!archive_.read(game.entry->std_name, std_bytes)) { failure = archive_.error; return false; }
    if (!game.stage.load(std_bytes.data(), u32(std_bytes.size()), globals.stage)) { failure = game.stage.error; return false; }
    AnmResource* stage_anm = nullptr;
    if (!load_anm(game.stage.std.anm_name.c_str(), stage_anm)) return false;
    const int slot = 3 + (globals.stage & 1);
    game.stage.anm = animations.load(slot, *stage_anm);
    if (game.stage.anm == nullptr) { failure = "stage anm load failed"; return false; }
    game.stage_live = true;

    // 0x4097c0：关卡更新链（优先级 0x11）；0x42cb60：GameManager 更新链（优先级 0xf）
    stage_update_chain_ = chains.create([this]() -> ChainResult {
        game.stage.update(animations, supervisor.game_speed);
        return ChainResult::Continue;
    }, 0x11, true);
    game_update_chain_ = chains.create([this]() -> ChainResult {
        return game.update(animations, globals, supervisor.game_speed) ? ChainResult::Continue : ChainResult::BreakExit;
    }, 0xf, true);
    // 0x42cb60 末：+0x90 = 0x3c、清 flags&4、0x418420 启用链
    game.transition_length = 0x3c;
    game.flags &= ~GameManager::kLoading;
    game.timer.set(0);
    chains.enable(game_update_chain_);
    chains.enable(stage_update_chain_);
    game_live = true;
    return true;
}

void GameSession::leave_game() {
    // 0x42d200：摘链、释放管理器；关卡背景 ANM 槽回收
    if (game_update_chain_) { chains.remove(game_update_chain_); game_update_chain_ = nullptr; }
    if (stage_update_chain_) { chains.remove(stage_update_chain_); stage_update_chain_ = nullptr; }
    if (game.stage_live) {
        const int slot = 3 + (game.stage.stage_number & 1);
        animations.kill_file(slot);
        animations.tick_world();
        animations.unload(slot);
    }
    game = GameManager{};
    game_live = false;
}

bool GameSession::logic_step() {
    sample_input();

    SceneId leave = SceneId::Invalid, enter = SceneId::Invalid;
    if (supervisor.consume_switch(leave, enter)) {
        if (leave == SceneId::Title) leave_title();
        if (enter == SceneId::Title && !enter_title_assets()) return false;
        if (leave == SceneId::Game) leave_game();
        if (enter == SceneId::Quit) quit_requested = true;
        if (enter == SceneId::Game && !enter_game()) return false;
    }

    // 启动幕：绘制链推进 boot_frames；标题线程等待条件（0x44abea / 0x44ac00）
    if (!supervisor.title_menu_live) {
        chains.run_draw();
        if (supervisor.title_assets_ready && supervisor.boot_hold_elapsed()) {
            if (!activate_title_menu()) return false;
        }
    }

    chains.run_update();
    animations.game_speed = supervisor.game_speed;
    animations.tick_world();
    return failure.empty();
}

}  // namespace th16
