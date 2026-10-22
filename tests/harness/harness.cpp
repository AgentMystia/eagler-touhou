// WASI 无头 harness：同一份 game 源码在 wasm32-wasip1 下独立运行，
// 供 CI 中的 oracle 对照（原版 Unicorn 轨迹 vs C++ 行为）调用。
// 编译方式见 portable/build-harness.mjs（WASI_SDK_PATH 需指向 wasi-sdk）。
// 用法：
//   th13-harness              旧有 RNG 冒烟（8 值输出）
//   th13-harness stage1 <资源目录> <事件输出路径> [帧数] [stage.ecl]
//     资源目录需含 st01.ecl 及其 ECLI/ANIM 依赖（artifacts/reference/raw）。
//   th13-harness replay <rpy 路径> <资源目录> <事件输出路径> [帧数] [stage 索引]
//     解码原版录像并把输入逐帧注入 StageBattle（0x448200 回放节奏），按
//     event-contract.md §7 落 player/kill/boss_op 事件；窗口自定（Boss 第一
//     非符 +300 帧或输入耗尽）。资源目录另需 plXXa.sht（移速源，§7.2）。
//   th13-harness draws <资源目录> <绘制输出路径> [帧数=3600] [stage.ecl]
//   th13-harness draws-replay <rpy> <资源目录> <绘制输出路径> [帧数] [stage 索引]
//     语义绘制清单导出（draw-contract.md v1 移植侧，TH13_DRAW_CAPTURE 构建）：
//     无输入窗 3600 帧 / 回放窗（与 oracle/render_oracle.py 同窗同帧号）。
//     帧序镜像 web 调度：副池步进 → ECL/battle → 表现层 sync → 主池步进 →
//     逐层（0..32）+ 战斗直绘 4 pass（debug_pass_mask=4 仅敌机，对齐 oracle
//     窗口径：mgr 两池精灵；HUD/STD/自机不在窗）。帧 0 = 装载趟渲染（不步进）。
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <memory>
#include <string>
#include <vector>

#include "game/AnmManager.hpp"
#include "game/AnmRenderer.hpp"
#include "game/DrawCapture.hpp"
#include "game/EclResource.hpp"
#include "game/GameInput.hpp"
#include "game/ReplayFile.hpp"
#include "game/Rng.hpp"
#include "game/ScreenViews.hpp"
#include "game/StageBattle.hpp"
#include "game/StagePresentation.hpp"
#include "game/StageSim.hpp"

namespace {

using th13::i32;
using th13::u32;

int rng_smoke() {
    th13::Rng rng(1);  // MSVC CRT 默认 srand(1)
    std::printf("th13-harness rng");
    for (int i = 0; i < 8; i++) {
        std::printf(" %d", rng.next());
    }
    std::printf("\n");
    return 0;
}

int stage1(const char* resource_dir, const char* event_path, unsigned frames,
           const char* stage) {
    th13::StageSim sim;
    sim.anm_effects_noop = true;  // 事件契约：307/308 与 stage1 oracle NOOP 同源
    if (!sim.load(resource_dir, event_path, stage)) {
        std::printf("stage1 load failed: %s\n", sim.error().c_str());
        return 1;
    }
    const bool ok = sim.run(frames);
    std::printf("stage1 frames=%u enemies=%u events=%llu ok=%d\n", frames, sim.spawn_count(),
                static_cast<unsigned long long>(sim.event_count()), ok ? 1 : 0);
    if (!ok) {
        std::printf("stage1 error: %s\n", sim.error().empty() ? "event sink failure" : sim.error().c_str());
        return 1;
    }
    return 0;
}

// 仓库文件读取（与 StageSim.cpp 的 read_file 同源约定）。
bool read_whole_file(const std::string& path, std::vector<th13::u8>& out) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    out.resize(size_t(size < 0 ? 0 : size));
    const size_t got = out.empty() ? 1 : std::fread(out.data(), 1, out.size(), file);
    std::fclose(file);
    return got == out.size();
}

// replay 模式的资源供给与宿主回调（无头：HUD/终局全部记账不打扰）。
// animation() 同 RunSceneAssets：核验存在并登记 sim.anim_list（begin() 装载
// 契约 anim_list ≥ 2）。
class ReplayResources final : public th13::EclResourceProvider {
public:
    std::string dir;
    th13::StageSim* sim = nullptr;
    bool read(const std::string& name, std::vector<th13::u8>& data) override {
        return read_whole_file(dir + "/" + name, data);
    }
    bool animation(u32, const std::string& name) override {
        std::vector<th13::u8> probe;
        if (!read_whole_file(dir + "/" + name, probe)) return false;
        if (sim) sim->anim_list.push_back(name);
        return true;
    }
};

class ReplayHost final : public th13::StageBattleServices {
public:
    bool life_hud(i32, i32) override { return true; }
    bool bomb_hud(i32, i32) override { return true; }
    bool request_game_over() override {
        game_over = true;
        return true;
    }
    bool hud_sound(i32) override { return true; }
    bool game_over = false;
};

int replay(const char* replay_path, const char* resource_dir, const char* event_path,
           unsigned frames, unsigned stage_index) {
    std::vector<th13::u8> bytes;
    if (!read_whole_file(replay_path, bytes)) {
        std::printf("replay read failed: %s\n", replay_path);
        return 1;
    }
    th13::ReplayFile replay_file;
    if (!replay_file.decode(bytes.data(), bytes.size())) {
        std::printf("replay decode failed: %s\n", replay_file.error().c_str());
        return 1;
    }
    if (stage_index >= replay_file.stages.size()) {
        std::printf("replay stage index %u out of %u\n", stage_index,
                    unsigned(replay_file.stages.size()));
        return 1;
    }
    const th13::ReplayStage& stage = replay_file.stages[stage_index];
    std::printf("replay name=%s rank=%d chara=%d shot=%d score=%d stages=%u\n",
                replay_file.player_name.c_str(), replay_file.rank, replay_file.character,
                replay_file.shot_type,
                replay_file.score, unsigned(replay_file.stages.size()));
    std::printf("replay stage=%u frames=%u events=%u score=%d\n", unsigned(stage.number),
                unsigned(stage.frame_count), unsigned(stage.events.size()), stage.score);

    ReplayResources provider;
    provider.dir = resource_dir;
    ReplayHost host;
    th13::StageBattle battle;
    provider.sim = &battle.sim;
    th13::SessionState progress;
    th13::HudPlayerState player;
    th13::HudScoreState score_state;
    // 注入观察不模拟终局：近似判定下的死亡按无敌复活继续（命数给足）。
    player.extra_lives = 99;
    char stage_file[16];
    std::snprintf(stage_file, sizeof stage_file, "st%02u.ecl", unsigned(stage.number));
    char sht_file[16];
    std::snprintf(sht_file, sizeof sht_file, "pl%02d%c.sht", replay_file.character,
                  replay_file.shot_type == 0 ? 'a' : 'b');
    battle.sim.anm_effects_noop = true;  // 事件契约：307/308 与 stage1 oracle NOOP 同源
    if (!battle.load(provider, stage_file, 1 << replay_file.rank, host, progress, player,
                     score_state, event_path, sht_file)) {
        std::printf("replay battle load failed: %s\n", battle.error().c_str());
        return 1;
    }

    // 逐帧注入（0x448200：态 → 0x4e4c08，沿由 0x414f30 幂等重算）。
    // 窗口：Boss 第一非符（§7.3）+300 帧、输入耗尽或显式帧数上限。
    th13::ReplayInputCursor cursor(stage);
    th13::GameInput input;
    const u32 total = frames ? th13::u32(frames) : stage.frame_count;
    u32 done = 0;
    i32 boss_frame = -1;
    bool boss_seen = false;
    bool nonspell_seen = false;
    u32 stop_after = 0xFFFFFFFFu;
    while (done < total && cursor.valid() && !host.game_over) {
        const th13::ReplayFrame frame = cursor.next();
        input.update(frame.state);
        if (!battle.step(input)) {
            std::printf("replay error at frame %u: %s\n", done, battle.error().c_str());
            return 1;
        }
        ++done;
        const bool boss = battle.sim.boss() != nullptr;
        if (boss && !boss_seen) {
            boss_seen = true;
            boss_frame = i32(done) - 1;  // 事件帧域：done-1 = 本步帧号
        }
        if (battle.sim.boss_nonspell_frame >= 0 && !nonspell_seen) {
            nonspell_seen = true;
            stop_after = done + 300;  // 非符锚点 +300 帧
        }
        if (done >= stop_after) break;
        if (done % 600 == 0)
            std::printf("replay frame=%u held=%04x x=%.1f y=%.1f enemies=%u slow=%u\n", done,
                        frame.state, battle.player_x, battle.player_y,
                        unsigned(battle.sim.enemies.size()), cursor.current_slow_rate());
    }
    std::printf("replay done frames=%u/%u x=%.1f y=%.1f deaths=%d score=%d boss_frame=%d "
                "nonspell_frame=%d\n",
                done, unsigned(stage.frame_count), battle.player_x, battle.player_y,
                player.deaths, score_state.score, boss_frame,
                battle.sim.boss_nonspell_frame);
    std::printf("stage1-events-replay frames=%u enemies=%u events=%llu nonspell=%d ok=1\n", done,
                battle.sim.spawn_count(),
                static_cast<unsigned long long>(battle.sim.event_count()),
                battle.sim.boss_nonspell_frame);
    return 0;
}

// ---- 语义绘制清单导出（draw-contract.md v1 移植侧导出器驱动）----

// 装载序对齐 oracle 最小战斗集（draw-contract §0）+ web begin_run bank 面：
// effect.anm → bank 8（mgr 槽 8）、st01.ecl ANIM 名单合并 → bank 4（名单序
// 槽 10/11）、bullet.anm → bank 5（mgr 槽 7）、plXX.anm → bank 6（窗口外，
// 身份槽 -1 可见标记）。纹理身份 (槽, entry=文件内序号, 名) 供 tex 字段。
constexpr th13::i32 kDrawsStageBank = 4, kDrawsBulletBank = 5, kDrawsPlayerBank = 6,
                    kDrawsEffectBank = 8;

class DrawsProvider final : public th13::EclResourceProvider {
public:
    std::string dir;
    th13::StageSim* sim = nullptr;
    bool read(const std::string& name, std::vector<th13::u8>& data) override {
        return read_whole_file(dir + "/" + name, data);
    }
    bool animation(u32, const std::string& name) override {
        std::vector<th13::u8> probe;
        if (!read_whole_file(dir + "/" + name, probe)) return false;
        if (sim) sim->anim_list.push_back(name);
        return true;
    }
};

class DrawsHost final : public th13::StageBattleServices {
public:
    bool life_hud(i32, i32) override { return true; }
    bool bomb_hud(i32, i32) override { return true; }
    bool request_game_over() override {
        game_over = true;
        return true;
    }
    bool hud_sound(i32) override { return true; }
    bool game_over = false;
};

struct DrawsSession {
    struct StageFileRange {
        th13::u32 first = 0, count = 0;
        th13::i32 slot = -1;
    };
    th13::DrawCapture capture;
    th13::AnmEnvironment environment;
    // battle 先于 animations：ANM 与 ECL 共享同一条 0x472c90 游戏侧 RNG
    // （原版全局态 0x4dc31c；oracle 实证装载期 2 调 + 每神灵 spawn 10 调，
    // ECL 与 ANM 本就是同一流——先前独立 visual 实例是移植侧偏差）。
    th13::StageBattle battle;
    th13::AnmManager animations{battle.sim.random, environment};
    th13::AnmRenderer renderer{capture};
    th13::ScreenViews views{renderer, environment};
    std::unique_ptr<th13::StagePresentation> presentation;
    std::vector<StageFileRange> stage_files;
    std::string error;

    bool fail(const std::string& why) {
        if (error.empty()) error = why;
        return false;
    }
    // web 对局期环境口径（ApplicationState::initialize 的 battle 子集）。
    void configure_environment() {
        environment.screen_width = 640;
        environment.screen_height = 480;
        environment.raster_scale = 1;
        environment.resolution_scale = 1;
        environment.render_target_has_alpha = true;
        environment.screen_offsets = {320, 16, 320, 16};
        environment.playfield_origin = {128, 16};
    }
    bool load_banks(DrawsProvider& provider, const char* player_anm);
    // 一帧渲染（draw-contract §0 帧序的移植侧镜像；ECL/battle 步进由调用方
    // 在本函数前完成）。
    bool render_frame(unsigned frame, th13::u32 held);
};

bool DrawsSession::load_banks(DrawsProvider& provider, const char* player_anm) {
    std::vector<th13::u8> bytes;
    // 装载序对齐 oracle 最小战斗集（draw-contract §0 / render_oracle run()）：
    // bullet（0x40d920 弹幕管理器先装）→ effect（mgr 槽 8）→ st01 ANIM 名单。
    // 模板 tick 共享 0x472c90 RNG，装载序错位会移动共享流（装载期 2 调的
    // 归属文件序即消费序）。ascii 不装（oracle 窗外资产；模板 tick 会抽签）；
    // 兜底精灵源 = bullet.anm（原版 game_main+0x19194 同源）。
    if (!read_whole_file(provider.dir + "/bullet.anm", bytes) ||
        !animations.load(kDrawsBulletBank, bytes.data(), th13::u32(bytes.size())))
        return fail("bullet.anm load failed: " + animations.error);
    if (auto* file = animations.resource(kDrawsBulletBank))
        capture.register_texture(file, 0, th13::u32(file->textures.size()), 7);
    environment.fallback_sprite_resource = animations.resource(kDrawsBulletBank);
    // effect.anm（mgr 槽 8；全局 ANM，oracle 0x46df40(EAX=8) 同位）。
    if (!read_whole_file(provider.dir + "/effect.anm", bytes) ||
        !animations.load(kDrawsEffectBank, bytes.data(), th13::u32(bytes.size())))
        return fail("effect.anm load failed: " + animations.error);
    if (auto* file = animations.resource(kDrawsEffectBank))
        capture.register_texture(file, 0, th13::u32(file->textures.size()), 8);
    // st01.ecl ANIM 名单合并 bank（web begin_run 同式；名单序 = mgr 槽 10 起）。
    std::vector<th13::u8> ecl;
    if (!read_whole_file(provider.dir + "/st01.ecl", ecl))
        return fail("st01.ecl missing");
    th13::EclResource head;
    if (!head.open(ecl.data(), th13::u32(ecl.size())) || head.animations.empty())
        return fail("st01.ecl ANIM list unavailable");
    th13::AnmResource merged;
    for (const auto& name : head.animations) {
        if (!read_whole_file(provider.dir + "/" + name, bytes))
            return fail("stage ANM missing: " + name);
        th13::AnmResource single;
        if (!single.open(bytes.data(), th13::u32(bytes.size())))
            return fail("stage ANM invalid: " + name);
        StageFileRange range;
        range.first = th13::u32(merged.textures.size());
        range.count = th13::u32(single.textures.size());
        range.slot = th13::i32(10 + stage_files.size());
        if (!merged.append(bytes.data(), th13::u32(bytes.size())))
            return fail("stage ANM append failed: " + name);
        stage_files.push_back(range);
    }
    if (!animations.adopt(kDrawsStageBank, std::move(merged)))
        return fail("stage ANM bank failed: " + animations.error);
    if (auto* file = animations.resource(kDrawsStageBank))
        for (const auto& range : stage_files)
            capture.register_texture(file, range.first, range.count, range.slot);
    // plXX.anm（自机 bank：oracle 窗口外的表现资产；身份槽 -1 可见标记）。
    if (!read_whole_file(provider.dir + "/" + std::string(player_anm), bytes) ||
        !animations.load(kDrawsPlayerBank, bytes.data(), th13::u32(bytes.size())))
        return fail(std::string("player ANM load failed: ") + player_anm);
    if (auto* file = animations.resource(kDrawsPlayerBank))
        capture.register_texture(file, 0, th13::u32(file->textures.size()), -1);
    return true;
}

bool DrawsSession::render_frame(unsigned frame, th13::u32 held) {
    if (!error.empty()) return false;
    capture.begin_frame(frame);
    // web 调度序：副池步进（@9）→ 场景 update（ECL + 表现层 sync，@20）→
    // 主池步进（@34）→ Draw pass。ECL/battle 步进由调用方在本函数前完成。
    if (!animations.update(true)) return fail("fore pool update: " + animations.error);
    if (!presentation->sync(battle, held)) return fail("presentation sync: " + presentation->error());
    if (!animations.update(false)) return fail("main pool update: " + animations.error);
    if (!views.camera(th13::DrawCamera::Fullscreen, false))
        return fail("full-screen camera unavailable");
    for (th13::u32 layer = 0; layer < 33; layer++)  // th13 层域：主链 0-30 + 副链 31/32
        if (!renderer.draw_layer(animations.registry.layer(layer)))
            return fail("animation layer draw: " + renderer.error);
    for (th13::u32 pass = 0; pass < 4; pass++)
        if (!presentation->draw(pass)) return fail("presentation draw: " + presentation->error());
    renderer.flush();
    capture.clear_device_texture();  // 0x45d2f0:49 帧尾 SetTexture(0,0)（UG 缓存不失效）
    if (!capture.end_frame()) return fail("draws write: " + capture.error());
    return true;
}

int run_draws(const char* resource_dir, const char* draws_path, unsigned frames,
              const char* stage) {
    DrawsSession session;
    session.configure_environment();
    DrawsProvider provider;
    provider.dir = resource_dir;
    provider.sim = &session.battle.sim;  // sim.begin 装载契约：anim_list ≥ 2
    DrawsHost host;
    th13::SessionState progress;
    th13::HudPlayerState player;
    th13::HudScoreState score;
    th13::draw_capture = &session.capture;
    const std::string stage_file = stage ? stage : "st01.ecl";
    if (!session.battle.load(provider, stage_file.c_str(), 1 << 3, host, progress, player,
                             score, nullptr, nullptr)) {
        std::printf("draws battle load failed: %s\n", session.battle.error().c_str());
        return 1;
    }
    if (!session.load_banks(provider, "pl00.anm")) {
        std::printf("draws banks failed: %s\n", session.error.c_str());
        return 1;
    }
    session.presentation = std::make_unique<th13::StagePresentation>(
        session.animations, session.renderer, th13::StagePresentation::Banks{
                                                   kDrawsStageBank, kDrawsBulletBank,
                                                   kDrawsPlayerBank, kDrawsEffectBank},
        31);
    th13::StagePresentation::debug_pass_mask = 4;  // 仅敌机（oracle 窗口径）
    // 无输入窗玩家口径：静态 (192,420)（oracle stage1 env 契约常量；不走
    // battle 自机模型——ECL 瞄准读 sim.player_x/y 默认值即此）。
    session.battle.player_x = 192.0f;
    session.battle.player_y = 420.0f;
    if (!session.capture.open_output(draws_path)) {
        std::printf("draws output failed: %s\n", session.capture.error().c_str());
        return 1;
    }
    if (!session.render_frame(0, 0)) {  // 帧 0 = 装载趟（root 构造内首趟已跑）
        std::printf("draws frame 0 failed: %s\n", session.error.c_str());
        return 1;
    }
    for (unsigned frame = 1; frame < frames; frame++) {
        if (!session.battle.sim.step_frame()) {
            std::printf("draws error at frame %u: %s\n", frame,
                        session.battle.sim.error().c_str());
            return 1;
        }
        if (!session.render_frame(frame, 0)) {
            std::printf("draws error at frame %u: %s\n", frame, session.error.c_str());
            return 1;
        }
        if (frame % 600 == 0)
            std::printf("draws frame=%u enemies=%u ops=%llu\n", frame,
                        unsigned(session.battle.sim.enemies.size()),
                        static_cast<unsigned long long>(
                            session.capture.quads() + session.capture.ups()));
    }
    session.capture.close_output();
    std::printf("port-draws frames=%llu quads=%llu ups=%llu ops=%llu zero=%u\n",
                static_cast<unsigned long long>(session.capture.frames_out()),
                static_cast<unsigned long long>(session.capture.quads()),
                static_cast<unsigned long long>(session.capture.ups()),
                static_cast<unsigned long long>(session.capture.quads() +
                                                session.capture.ups()),
                session.capture.zero_op_frames());
    return 0;
}

int run_draws_replay(const char* replay_path, const char* resource_dir, const char* draws_path,
                     unsigned frames, unsigned stage_index) {
    std::vector<th13::u8> bytes;
    if (!read_whole_file(replay_path, bytes)) {
        std::printf("draws-replay read failed: %s\n", replay_path);
        return 1;
    }
    th13::ReplayFile replay_file;
    if (!replay_file.decode(bytes.data(), bytes.size())) {
        std::printf("draws-replay decode failed: %s\n", replay_file.error().c_str());
        return 1;
    }
    if (stage_index >= replay_file.stages.size()) {
        std::printf("draws-replay stage index %u out of %u\n", stage_index,
                    unsigned(replay_file.stages.size()));
        return 1;
    }
    const th13::ReplayStage& stage = replay_file.stages[stage_index];
    std::printf("replay name=%s rank=%d chara=%d shot=%d score=%d stages=%u\n",
                replay_file.player_name.c_str(), replay_file.rank, replay_file.character,
                replay_file.shot_type, replay_file.score, unsigned(replay_file.stages.size()));
    DrawsSession session;
    session.configure_environment();
    DrawsProvider provider;
    provider.dir = resource_dir;
    provider.sim = &session.battle.sim;
    DrawsHost host;
    th13::SessionState progress;
    th13::HudPlayerState player;
    th13::HudScoreState score;
    player.extra_lives = 99;  // 观察口径：近似判定的死亡不终局（replay 模式同款）
    progress.difficulty = replay_file.rank & 3;
    char stage_file[16];
    std::snprintf(stage_file, sizeof stage_file, "st%02u.ecl", unsigned(stage.number));
    char sht_file[16];
    std::snprintf(sht_file, sizeof sht_file, "pl%02d%c.sht", replay_file.character,
                  replay_file.shot_type == 0 ? 'a' : 'b');
    char player_anm[16];
    std::snprintf(player_anm, sizeof player_anm, "pl%02d.anm", replay_file.character & 3);
    th13::draw_capture = &session.capture;
    if (!session.battle.load(provider, stage_file, 1 << (replay_file.rank & 3), host, progress,
                             player, score, nullptr, sht_file)) {
        std::printf("draws-replay battle load failed: %s\n", session.battle.error().c_str());
        return 1;
    }
    if (!session.load_banks(provider, player_anm)) {
        std::printf("draws-replay banks failed: %s\n", session.error.c_str());
        return 1;
    }
    session.presentation = std::make_unique<th13::StagePresentation>(
        session.animations, session.renderer, th13::StagePresentation::Banks{
                                                   kDrawsStageBank, kDrawsBulletBank,
                                                   kDrawsPlayerBank, kDrawsEffectBank},
        31);
    th13::StagePresentation::debug_pass_mask = 4;
    if (!session.capture.open_output(draws_path)) {
        std::printf("draws-replay output failed: %s\n", session.capture.error().c_str());
        return 1;
    }
    // 帧号与 oracle 同窗：帧 0 = 装载趟渲染；帧 f（≥1）消费输入 f-1。
    const unsigned total = frames && frames < stage.frame_count ? frames
                                                               : unsigned(stage.frame_count);
    th13::ReplayInputCursor cursor(stage);
    th13::GameInput input;
    if (!session.render_frame(0, 0)) {
        std::printf("draws-replay frame 0 failed: %s\n", session.error.c_str());
        return 1;
    }
    for (unsigned frame = 1; frame < total; frame++) {
        if (!cursor.valid()) {
            std::printf("draws-replay input exhausted at frame %u (window %u)\n", frame, total);
            return 1;
        }
        input.update(cursor.next().state);
        if (!session.battle.step(input)) {
            std::printf("draws-replay error at frame %u: %s\n", frame,
                        session.battle.error().c_str());
            return 1;
        }
        if (host.game_over) {
            std::printf("draws-replay game over at frame %u (window %u)\n", frame, total);
            return 1;
        }
        if (!session.render_frame(frame, input.held)) {
            std::printf("draws-replay error at frame %u: %s\n", frame, session.error.c_str());
            return 1;
        }
        if (frame % 600 == 0)
            std::printf("draws-replay frame=%u enemies=%u ops=%llu\n", frame,
                        unsigned(session.battle.sim.enemies.size()),
                        static_cast<unsigned long long>(
                            session.capture.quads() + session.capture.ups()));
    }
    session.capture.close_output();
    std::printf("port-draws-replay frames=%llu quads=%llu ups=%llu ops=%llu zero=%u\n",
                static_cast<unsigned long long>(session.capture.frames_out()),
                static_cast<unsigned long long>(session.capture.quads()),
                static_cast<unsigned long long>(session.capture.ups()),
                static_cast<unsigned long long>(session.capture.quads() +
                                                session.capture.ups()),
                session.capture.zero_op_frames());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 4 && std::strcmp(argv[1], "stage1") == 0)
        // stage1 <资源目录> <输出> [frames] [stage.ecl]
        return stage1(argv[2], argv[3], argc >= 5 ? unsigned(std::atoi(argv[4])) : 3600u,
                      argc >= 6 ? argv[5] : nullptr);
    if (argc >= 5 && std::strcmp(argv[1], "replay") == 0)
        // replay <rpy> <资源目录> <事件输出> [帧数] [stage 索引]
        return replay(argv[2], argv[3], argv[4], argc >= 6 ? unsigned(std::atoi(argv[5])) : 0u,
                      argc >= 7 ? unsigned(std::atoi(argv[6])) : 0u);
    if (argc >= 4 && std::strcmp(argv[1], "draws") == 0)
        // draws <资源目录> <绘制输出> [帧数] [stage.ecl]
        return run_draws(argv[2], argv[3], argc >= 5 ? unsigned(std::atoi(argv[4])) : 3600u,
                         argc >= 6 ? argv[5] : nullptr);
    if (argc >= 5 && std::strcmp(argv[1], "draws-replay") == 0)
        // draws-replay <rpy> <资源目录> <绘制输出> [帧数] [stage 索引]
        return run_draws_replay(argv[2], argv[3], argv[4],
                                argc >= 6 ? unsigned(std::atoi(argv[5])) : 0u,
                                argc >= 7 ? unsigned(std::atoi(argv[6])) : 0u);
    return rng_smoke();
}
