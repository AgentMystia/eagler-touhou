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

bool GameSession::load_boot_banks() {
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
    // 0x418a30：EffectManager 持 effect.anm / bullet.anm，并把 VM 回调（充能环等）挂到 AnmManager
    effects.create(animations, animations.file(8), animations.file(7));
    effect_update_chain_ = chains.create([this]() -> ChainResult {
        effects.sweep();
        return ChainResult::Continue;
    }, 0x1f, true);
    chains.enable(effect_update_chain_);
    // 0x43adc0 → 0x4497e0 → 0x449720（ScoreFile 构造）：对 DAT_004a6d88 做 0x1ee 次单步随机（0 -> 0x41b6）
    for (int i = 0; i < 0x1ee; ++i) animations.visual_rng.next16();
    return true;
}

bool GameSession::begin_boot() {
    if (!load_boot_banks()) return false;

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

bool GameSession::start_game_direct() {
    if (!load_boot_banks()) return false;
    supervisor.current = SceneId::Game;
    supervisor.requested = SceneId::Game;
    return enter_game();
}

bool GameSession::create_player() {
    // 0x42cb60 新游戏段（DAT_004c17d0 != 0）
    // 残机：模式 0 → 2；符卡练习 → 0（炸弹亦 0）；练习 → DAT_004a5bf8 ? 其值-1 : 9
    const i32 mode = globals.mode();
    // 得分 / 擦弹清零；得点值初值与上限按难度表（DAT_004922a8 / DAT_00492290 × 100；Extra 为 100000 / 1000000）
    static constexpr i32 kPointInitial[6] = {10000, 10000, 10000, 10000, 10000, 100000};
    static constexpr i32 kPointMax[6] = {500000, 500000, 500000, 500000, 500000, 1000000};
    const int diff = globals.difficulty < 0 ? 0 : globals.difficulty > 5 ? 5 : globals.difficulty;
    globals.graze = 0;
    globals.score = 0;
    globals.point_value_initial = kPointInitial[diff] * 100;
    globals.point_value_max = kPointMax[diff] * 100;
    globals.point_value = i32((double(float(globals.point_value_initial)) / 100.0) * 100.0);
    if (mode == 2) { globals.lives = 0; globals.bombs = 0; }
    else if (mode == 0) globals.lives = 2;
    else globals.lives = 9;

    // 0x440fb0：pl0N.anm → 槽 9（DAT_00492ccc[shot+aux]）、pl0Nsub.anm → 槽 0x1e（DAT_00492cdc[season]）、.sht 两份
    static constexpr const char* kPlayerAnm[4] = {"pl00.anm", "pl02.anm", "pl03.anm", "pl01.anm"};
    static constexpr const char* kPlayerSubAnm[5] = {"pl00sub.anm", "pl02sub.anm", "pl03sub.anm", "pl01sub.anm", "pl04sub.anm"};
    static constexpr const char* kPlayerSht[4] = {"pl00.sht", "pl02.sht", "pl03.sht", "pl01.sht"};
    static constexpr const char* kPlayerSubSht[5] = {"pl00sub.sht", "pl02sub.sht", "pl03sub.sht", "pl01sub.sht", "pl04sub.sht"};
    const int shot = globals.shot + globals.shot_aux;
    if (shot < 0 || shot > 3 || globals.season < 0 || globals.season > 4) { failure = "player shot/season out of range"; return false; }
    AnmResource* pl_res = nullptr;
    AnmResource* sub_res = nullptr;
    if (!load_anm(kPlayerAnm[shot], pl_res)) return false;
    if (!load_anm(kPlayerSubAnm[globals.season], sub_res)) return false;
    AnmFile* pl_file = animations.load(9, *pl_res);
    AnmFile* sub_file = animations.load(0x1e, *sub_res);
    if (pl_file == nullptr || sub_file == nullptr) { failure = "player anm load failed"; return false; }
    std::vector<u8> sht_bytes, sub_sht_bytes;
    if (!archive_.read(kPlayerSht[shot], sht_bytes)) { failure = archive_.error; return false; }
    if (!archive_.read(kPlayerSubSht[globals.season], sub_sht_bytes)) { failure = archive_.error; return false; }
    if (!game.player.init(animations, globals, pl_file, sub_file, sht_bytes.data(), u32(sht_bytes.size()),
                          sub_sht_bytes.data(), u32(sub_sht_bytes.size()))) { failure = game.player.error; return false; }
    game.player_live = true;

    // 火力 / 季节量表初值
    const i32 per = globals.power_per_level, max = globals.power_max;
    auto clamp_full = [&]() {
        globals.power = per * 4;
        if (globals.power <= max) { if (globals.power < per) globals.power = per; }
        else globals.power = max;
    };
    if (mode == 2) {
        clamp_full();
        globals.season_gauge = globals.season_gauge_max < 0x186 ? globals.season_gauge_max : 0x186;
    } else if (globals.stage < 2) {
        globals.power = per > max ? max : per;
        globals.season_gauge = globals.season_gauge_max < 0 ? globals.season_gauge_max : 0;
    } else {
        clamp_full();
        globals.season_gauge = globals.season_gauge_max < 0 ? globals.season_gauge_max : 0;
    }
    game.player.setup_options(animations, globals);   // 0x4440e0
    globals.flags &= ~0x4u;

    // 0x447fd0 Replay 记录链（优先级 0x10）：DAT_004a52c8 = held 低 16 位 → 0x418650 边沿
    replay_record_chain_ = chains.create([this]() -> ChainResult {
        game_input.held = supervisor.input.held & 0xffffu;
        game_input.commit();
        return ChainResult::Continue;
    }, 0x10, true);
    // 0x440fb0：自机更新链 0x16（0x443720 → 0x442560）与绘制链 0x1d（0x443730）
    player_update_chain_ = chains.create([this]() -> ChainResult {
        game.player.update(animations, globals, game_input, game.world, animations.file(8), geometry, supervisor.game_speed);
        harvest_player_sfx();
        game.player.sfx_stop_queue.clear();
        game.player.gui_events.clear();
        return ChainResult::Continue;
    }, 0x16, true);
    player_draw_chain_ = chains.create([this]() -> ChainResult {
        game.player.prepare_draw();
        return ChainResult::Continue;
    }, 0x1d, false);
    chains.enable(replay_record_chain_);
    chains.enable(player_update_chain_);
    chains.enable(player_draw_chain_);
    return true;
}

bool GameSession::enter_game() {
    // 0x42d700：flags |= 4、模式参数；0x42cb60 装载线程（此处同步）
    game = GameManager{};
    game.flags = GameManager::kLoading;
    game.mode = 0;
    game.entry = &stage_entry(globals.stage);
    game_input = InputState{};
    // 0x42cb60 → 0x449e50 → 0x449030（Replay 构造）：重置 DAT_004a6d8c 调用计数
    animations.visual_rng.calls = 0;

    // 0x42cb60：新游戏（DAT_004c17d0，0x43ce10 case 7 置位）先建 Player，再建 Gui / Stage / …
    if (!create_player()) return false;

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

    // 0x411dd0：BulletManager（bullet.anm 槽 7、effect.anm 槽 8）；更新链 0x1c（0x411880）、绘制链 0x25（0x411a30）
    {
        AnmFile* bullet_anm = animations.file(BulletManager::kBulletAnmSlot);
        if (bullet_anm == nullptr) { failure = "bullet.anm not loaded"; return false; }
        game.bullets = std::make_unique<BulletManager>();
        game.bullets->create(animations, *bullet_anm, animations.file(8), globals,
                             game.player_live ? &game.player : nullptr, &game.world, animations.visual_rng);
        game.bullets_live = true;
        bullet_update_chain_ = chains.create([this]() -> ChainResult {
            // 0x411880：GameManager 装载中（bit2）或结束位（bit0 经 >>2 映射）时跳过
            if (game.bullets_live && ((game.flags | (game.flags >> 2)) & 1u) == 0) {
                game.bullets->sfx_queue.clear();
                game.bullets->update(supervisor.game_speed, (game.flags & 0x400u) != 0);
                sfx_events.insert(sfx_events.end(), game.bullets->sfx_queue.begin(), game.bullets->sfx_queue.end());
                game.bullets->sfx_queue.clear();
                // 被弹（0x443f10）在弹幕链内登记的自机音效本帧即生效
                if (game.player_live) harvest_player_sfx();
            }
            return ChainResult::Continue;
        }, 0x1c, true);
        bullet_draw_chain_ = chains.create([this]() -> ChainResult {
            if (game.bullets_live && (game.flags & GameManager::kLoading) == 0) game.bullets->prepare_draw();
            return ChainResult::Continue;
        }, 0x25, false);
    }

    // 0x42f440：ItemManager；更新链 0x1d（0x4308f0 → 0x42f4e0）、绘制链 0x21 / 0x13（0x4307a0）
    {
        game.items = std::make_unique<ItemManager>();
        game.items->create(animations, animations.file(BulletManager::kBulletAnmSlot), animations.file(8), globals,
                           game.player_live ? &game.player : nullptr, animations.game_rng);
        game.items_live = true;
        game.bullets->items = game.items.get();
        item_update_chain_ = chains.create([this]() -> ChainResult {
            // 0x4308f0：GameManager 装载中（bit2）/ 结束位（bit0 经 >>2 映射）/ 0x400 冻结时跳过
            if (game.items_live && ((game.flags | (game.flags >> 2)) & 1u) == 0 && (game.flags & 0x400u) == 0) {
                game.items->dialogue = game.world.dialogue;
                game.items->focus_held = (game_input.held & 0x8u) != 0;
                game.items->update(supervisor.game_speed);
                sfx_events.insert(sfx_events.end(), game.items->sfx_queue.begin(), game.items->sfx_queue.end());
                game.items->sfx_queue.clear();
                // 0x44a460 弹出数字 / 0x42bcf0 Gui 通知：Ascii / Gui 接入前无状态影响
                game.items->popups.clear();
                game.items->gui_events.clear();
            }
            return ChainResult::Continue;
        }, 0x1d, true);
        item_draw_chain_ = chains.create([this]() -> ChainResult {
            if (game.items_live && (game.flags & GameManager::kLoading) == 0) game.items->prepare_draw(false);
            return ChainResult::Continue;
        }, 0x21, false);
        item_draw_season_chain_ = chains.create([this]() -> ChainResult {
            if (game.items_live && (game.flags & GameManager::kLoading) == 0) game.items->prepare_draw(true);
            return ChainResult::Continue;
        }, 0x13, false);
    }

    // 0x41b340 / 0x41ae70：装 ECL，登记敌人更新链优先级 0x1a（0x41b4f0→0x41b3d0）
    {
        auto read_ecl = [](void* user, const std::string& name, std::vector<u8>& data) -> bool {
            return static_cast<Archive*>(user)->read(name.c_str(), data);
        };
        auto load_enemy_anm = [](void* user, u32 slot, const std::string& name) -> bool {
            auto* self = static_cast<GameSession*>(user);
            AnmResource* res = nullptr;
            if (!self->load_anm(name.c_str(), res) || res == nullptr) return false;
            return self->animations.load(int(slot), *res) != nullptr;
        };
        game.enemies.read_user = &archive_;
        game.enemies.read_fn = read_ecl;
        game.enemies.anm_user = this;
        game.enemies.anm_fn = load_enemy_anm;
        game.enemies.bullets = game.bullets.get();
        game.enemies.items = game.items.get();
        game.enemies.effects = &effects;
        game.enemies.spellcard = &spellcard;
        game.player.spellcard = &spellcard;
        if (!game.enemies.create(game.entry->ecl_name, globals, game.player_live ? &game.player : nullptr,
                                 animations.visual_rng, animations, u8(1u << (globals.difficulty & 31)))) {
            failure = game.enemies.error;
            return false;
        }
        game.enemies_live = true;
        game.player.enemies = &game.enemies;
        game.player.items = game.items.get();
        game.player.effects = &effects;
        game.player.bullet_manager = game.bullets.get();
        game.player.global_game_speed = &supervisor.game_speed;
        game.enemies.global_game_speed = &supervisor.game_speed;
        enemy_update_chain_ = chains.create([this]() -> ChainResult {
            if (game.enemies_live) {
                game.enemies.sfx_queue.clear();
                if (game.bullets_live) {
                    game.bullets->game_speed = supervisor.game_speed;
                    game.bullets->sfx_queue.clear();
                }
                game.enemies.dialogue_active = game.world.dialogue;
                game.enemies.update(supervisor.game_speed);
                if (game.bullets_live) {
                    // ECL 601 等发射期间登记的音效（0x45e1f0 调用序在敌人链内）
                    sfx_events.insert(sfx_events.end(), game.bullets->sfx_queue.begin(), game.bullets->sfx_queue.end());
                    game.bullets->sfx_queue.clear();
                }
                if (sync_enemy_count) game.world.enemy_count = game.enemies.enemy_count;
                if (!game.enemies.error.empty()) {
                    failure = game.enemies.error;
                    return ChainResult::BreakExit;
                }
            }
            return ChainResult::Continue;
        }, 0x1a, true);
    }

    // 0x4178a0 / 0x417700：Spellcard（ECL 之后、Bomb 之后创建）；更新链 0x1e（0x417ee0 → 0x417930）、绘制链 0xc
    spellcard.reset();
    spellcard.animations = &animations;
    spellcard.enemies = &game.enemies;
    spellcard.player = game.player_live ? &game.player : nullptr;
    spellcard.globals = &globals;
    spellcard.stage = &game.stage;
    spellcard.entry = game.entry;
    spellcard_update_chain_ = chains.create([this]() -> ChainResult {
        spellcard.update(supervisor.game_speed);
        sfx_events.insert(sfx_events.end(), spellcard.sfx_queue.begin(), spellcard.sfx_queue.end());
        spellcard.sfx_queue.clear();
        gui_events.insert(gui_events.end(), spellcard.gui_events.begin(), spellcard.gui_events.end());
        spellcard.gui_events.clear();
        spellcard.gui_timer_events.clear();  // Gui+0x94 / +0x98 计时 VM 中断：Gui 接入时补
        return ChainResult::Continue;
    }, 0x1e, true);
    chains.enable(spellcard_update_chain_);

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
    if (bullet_update_chain_) chains.enable(bullet_update_chain_);
    if (bullet_draw_chain_) chains.enable(bullet_draw_chain_);
    if (item_update_chain_) chains.enable(item_update_chain_);
    if (item_draw_chain_) chains.enable(item_draw_chain_);
    if (item_draw_season_chain_) chains.enable(item_draw_season_chain_);
    if (enemy_update_chain_) chains.enable(enemy_update_chain_);
    game_live = true;
    return true;
}

void GameSession::leave_game() {
    // 0x42d200：摘链、释放管理器；关卡背景 ANM 槽回收
    if (game_update_chain_) { chains.remove(game_update_chain_); game_update_chain_ = nullptr; }
    if (stage_update_chain_) { chains.remove(stage_update_chain_); stage_update_chain_ = nullptr; }
    if (enemy_update_chain_) { chains.remove(enemy_update_chain_); enemy_update_chain_ = nullptr; }
    if (bullet_update_chain_) { chains.remove(bullet_update_chain_); bullet_update_chain_ = nullptr; }
    if (bullet_draw_chain_) { chains.remove(bullet_draw_chain_); bullet_draw_chain_ = nullptr; }
    if (item_update_chain_) { chains.remove(item_update_chain_); item_update_chain_ = nullptr; }
    if (item_draw_chain_) { chains.remove(item_draw_chain_); item_draw_chain_ = nullptr; }
    if (item_draw_season_chain_) { chains.remove(item_draw_season_chain_); item_draw_season_chain_ = nullptr; }
    if (spellcard_update_chain_) { chains.remove(spellcard_update_chain_); spellcard_update_chain_ = nullptr; }
    if (replay_record_chain_) { chains.remove(replay_record_chain_); replay_record_chain_ = nullptr; }
    if (player_update_chain_) { chains.remove(player_update_chain_); player_update_chain_ = nullptr; }
    if (player_draw_chain_) { chains.remove(player_draw_chain_); player_draw_chain_ = nullptr; }
    if (game.stage_live) {
        const int slot = 3 + (game.stage.stage_number & 1);
        animations.kill_file(slot);
        animations.tick_world();
        animations.unload(slot);
    }
    if (game.player_live) {
        // 0x441a50 析构：pl0N.anm / pl0Nsub.anm 的 VM 全部标删（0x46f270）并释放槽 9 / 0x1e
        animations.kill_file(9);
        animations.kill_file(0x1e);
        animations.tick_world();
        animations.unload(9);
        animations.unload(0x1e);
    }
    game = GameManager{};
    game_live = false;
}

void GameSession::harvest_player_sfx() {
    sfx_events.insert(sfx_events.end(), game.player.sfx_queue.begin(), game.player.sfx_queue.end());
    player_sfx_events.insert(player_sfx_events.end(), game.player.sfx_queue.begin(), game.player.sfx_queue.end());
    game.player.sfx_queue.clear();
}

bool GameSession::logic_step() {
    sample_input();
    sfx_events.clear();
    player_sfx_events.clear();
    gui_events.clear();

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
        if (run_draw_chains) chains.run_draw();
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
