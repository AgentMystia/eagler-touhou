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
//   th13-harness title <资源目录> <绘制输出路径>     （窗A：主菜单→难度，600 帧）
//   th13-harness title-b <资源目录> <绘制输出路径>   （窗B：难度→角色→开局转场）
//     标题窗语义绘制导出（oracle/title_oracle.py 同窗同规则）：帧序镜像 web
//     Application 启动序——装载幕（prepare_loading 首渲染 = 帧 0，boot 180 帧
//     推进 = 帧 1..180）→ TitleScene 步（= 帧 181 起，帧内序 controls →
//     scheduler.update（标题@6/副池@9/主池@34）→ scheduler.draw（AnmDrawSchedule
//     31 层 + 标题垫片@66））。全局帧概念与 oracle 0x45d2f0 帧循环 1:1 对齐
//     （oracle f0=开场创建当帧 ops=0 ↔ port 帧 0=装载幕首渲染）。输入按冻结
//     时刻表注入（controls u32 单帧：Z=0x01/↑=0x10/↓=0x20/←=0x40/→=0x80；
//     窗A f380↓/f420↑/f470Z；窗B f470Z/f530↓/f590Z/f650→/f710Z）。导出后按
//     oracle cut_window 同规则截窗：首个 ops>0 帧重编号 0 起；窗A 固定 600 帧，
//     窗B 止于开局转场请求帧（start_game → Destination::Run，含该帧）。
#include <cstdlib>
#include <cstring>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game/AnmDrawSchedule.hpp"
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
#include "game/TitleScene.hpp"
#include "game/TitleFrame.hpp"

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
    // 双流实证（oracle）：ANM 用全局 0x4dc31c（种子 0，装载 2 调 + 每神灵
    // 10 调）；ECL 的 [-9987] 族抽签在 f61 仍返回新流第 1 抽（-0.618 →
    // GirlRedA01 生成 x=-163.783264）——与 ANM 消费互不推进 ⟹ 两条独立
    // 种子 0 流（stage1 事件基线逐字节一致亦证 ECL 流无 ANM 污染）。
    // 曾误并为单流（轮 2）：f61 前恰好 ECL 零抽签故神灵仍匹配，f61 首抽
    // 即分歧（x=-154.101）。
    th13::GameRandom anm_random;  // ANM 侧流（原版 0x4dc31c 同构）
    th13::StageBattle battle;
    th13::AnmManager animations{anm_random, environment};
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
        environment.screen_offsets = {32, 16, 224, 16};
        environment.playfield_origin = {32, 16};
        // 0x43bbc0 战斗初始化后的相机/参考全局（oracle 同构）：变量 10018 =
        // 0x4dcb38(0)+0x4dcafc(1000.0)——神灵 z 链（ins_103 [10006]+=1000）对拍
        // 实证；10016/10017/10019-10021 均为 0。
        environment.camera_origin = {0, 0, 1000};
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
    // f159 取证（TH13_DUMPCHECK=帧号）：层 2 灵的插值通道字段
    //（world[12] = vm+0x3c；start/end/duration/timer 全量，位级 dump——
    // oracle 侧 trace_spawn 0x4692d0 watch 同字段对照）。
    if (const char* dump_at = std::getenv("TH13_DUMPCHECK")) {
        const unsigned at = unsigned(std::atoi(dump_at));
        if (frame == at) {
            const auto as_bits = [](float value) {
                return th13::u32(*reinterpret_cast<const th13::u32*>(&value));
            };
            for (th13::AnmVm* vm : animations.registry.layer(2)) {
                const auto& ch = vm->interpolators.position;
                std::printf(
                    "[f%u] s4aa=%d x3c=%.9g bits=%08x start=%08x,%08x,%08x "
                    "end=%08x,%08x,%08x dur=%08x mode=%d frac=%08x\n",
                    frame, vm->source_script,
                    double(vm->variables.position.x),
                    as_bits(vm->variables.position.x),
                    as_bits(ch.start[0]), as_bits(ch.start[1]),
                    as_bits(ch.start[2]), as_bits(ch.end[0]),
                    as_bits(ch.end[1]), as_bits(ch.end[2]),
                    as_bits(float(ch.duration)), ch.mode,
                    as_bits(ch.timer.fractional));
            }
        }
    }
    if (!views.camera(th13::DrawCamera::Fullscreen, false))
        return fail("full-screen camera unavailable");
    for (th13::u32 layer = 0; layer < 33; layer++)  // th13 层域：主链 0-30 + 副链 31/32
        if (!renderer.draw_layer(animations.registry.layer(layer)))
            return fail("animation layer draw: " + renderer.error);
    for (th13::u32 pass = 0; pass < 4; pass++)
        if (!presentation->draw(pass)) return fail("presentation draw: " + presentation->error());
    capture.set_stale_layer(56);  // 原版层循环 0..56 走完后的 watch 残值（UP 层口径）
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
    session.presentation->enemies_from_ecl = true;  // 敌机本体 VM 走 ECL 303/306
    session.battle.sim.ecl_slot_anm = true;
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
    session.presentation->enemies_from_ecl = true;  // 敌机本体 VM 走 ECL 303/306
    session.battle.sim.ecl_slot_anm = true;
    th13::StagePresentation::debug_pass_mask = 4;
    if (!session.capture.open_output(draws_path)) {
        std::printf("draws-replay output failed: %s\n", session.capture.error().c_str());
        return 1;
    }
    // 帧号与 oracle 同窗（render_oracle.py run_replay：帧 0..total 共 total+1
    // 行）：帧 0 = 装载趟渲染；帧 f（≥1）消费输入 f-1 并步进。oracle 侧循环
    // while frame < total: frame += 1 → 执行帧 1..total，末帧 = total——两侧
    // 帧→输入映射同构（帧 f ↔ 输入 f-1），本侧此前漏末帧 f=total（off-by-one，
    // 实测 11064 行 vs oracle 11065 行）。
    const unsigned total = frames && frames < stage.frame_count ? frames
                                                               : unsigned(stage.frame_count);
    th13::ReplayInputCursor cursor(stage);
    th13::GameInput input;
    if (!session.render_frame(0, 0)) {
        std::printf("draws-replay frame 0 failed: %s\n", session.error.c_str());
        return 1;
    }
    for (unsigned frame = 1; frame <= total; frame++) {
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

// ---- 标题窗语义绘制导出（title/title-b；oracle/title_oracle.py 同窗）----

// 标题期平台（无头）：音频/覆盖层全桩；destination 记账供窗B终点判定
// （title_probe.cpp 的 Host 同形 + start_game 观察口）。
class TitleHost final : public th13::TitleScenePlatform {
public:
    bool clear_title_overlay() override { return true; }
    bool queue_title_music(i32, i32, const std::string&) override { return true; }
    bool clear_current_wave() override { return true; }
    bool title_exit(i32 value) override {
        exit_destination = value;
        return true;
    }
    bool fade_out_title_music() override { return true; }
    bool sound(i32) override { return true; }
    bool begin_transition(u32& animation) override {
        animation = 0;
        return true;
    }
    bool start_game(i32 stage) override {
        requested_stage = stage;
        run_requested = true;  // Destination::Run 排定帧（ApplicationState::start_game 同语义）
        return true;
    }
    bool hud_text(const th13::HudTextDraw&) override { return true; }
    i32 exit_destination = -1;
    i32 requested_stage = 0;
    bool run_requested = false;
};

// 帧序与成员关系镜像 ApplicationState（装载幕隔离管理器 → 主管理器 7 bank →
// TitleScene + AnmDrawSchedule 同一 FrameScheduler）；析构走显式 teardown
// （~ApplicationState 的摘回调序），不依赖成员逆序。
// 注意：每个 AnmManager 内嵌 0x1fff 槽池（≈64KB），双管理器会话远超 wasm
// 默认栈——必须堆置（DrawsSession 单管理器恰好塞得下栈，本模式不行）。
struct TitleSession {
    th13::DrawCapture capture;
    th13::GameRandom loading_random;  // 装载幕隔离 RNG（ApplicationState::loading_random）
    th13::AnmEnvironment loading_environment;
    th13::AnmManager loading_animations{loading_random, loading_environment};
    th13::GameRandom anm_random;  // ANM 侧流（原版 0x4dc31c 种子 0 同构）
    th13::AnmEnvironment environment;
    th13::AnmManager animations{anm_random, environment};
    th13::AnmRenderer renderer{capture};
    th13::ScreenViews views{renderer, environment};
    th13::FrameScheduler scheduler;
    TitleHost host;
    th13::SessionState progress;
    th13::TitleSelectionSettings selection;
    std::unique_ptr<th13::AnmDrawSchedule> draw_schedule;
    std::unique_ptr<th13::TitleScene> title;
    std::array<th13::FrameCallback, 2> animation_updates{};
    th13::u32 loading_signature = 0;
    unsigned loading_frames = 0;
    std::string dir;
    std::string error;

    bool fail(const std::string& why) {
        if (error.empty()) error = why;
        return false;
    }
    bool read_bank(th13::AnmManager& manager, const char* name, th13::i32 bank) {
        std::vector<th13::u8> bytes;
        if (!read_whole_file(dir + "/" + name, bytes) ||
            !manager.load(bank, bytes.data(), th13::u32(bytes.size())))
            return fail(std::string("ANM load failed: ") + name + ": " + manager.error);
        if (auto* file = manager.resource(bank))
            capture.register_texture(file, 0, th13::u32(file->textures.size()), bank);
        return true;
    }
    // 装载幕（ApplicationState::prepare_loading + draw_loading(true) 镜像）：
    // sig+ascii 入隔离管理器、sig script0 建 VM、首渲染 = port 全局帧 0。
    bool prepare_loading() {
        loading_environment.resolution_scale = 1;
        loading_environment.raster_scale = 1;
        if (!read_bank(loading_animations, "sig.anm", 1) || !read_bank(loading_animations, "ascii.anm", 2) ||
            !read_bank(loading_animations, "text.anm", 0))  // [boot @R] 边条纹理身份（槽0 entry2/3）
            return false;
        loading_signature = loading_animations.create(1, 0);  // sig script0（两半拼 640）
        if (!loading_signature) return fail("loading signature create: " + loading_animations.error);
        // [窗A 修正镜像] sig 右半（script0 自 spawn 的 script1）层 = L26——与
        // Application.cpp draw_loading 同行（oracle boot 全程 f0-f178 实测 23/26；
        // dump ins_304(25) 的 +1 偏移，蓝图 §8.3）。
        if (auto* boot = loading_animations.registry.find(loading_signature))
            if (auto* right = loading_animations.registry.find_child_script(*boot, 1, 0))
                right->visual.layer = 26;
        loading_frames = 0;
        return render_loading_frame(0);
    }
    // 装载幕渲染（ApplicationState::render_loading 镜像；web 外壳品牌直绘
    // draw_startup_branding 为浏览器壳层物，不在语义清单，不镜像）。
    bool render_loading_frame(unsigned frame) {
        capture.begin_frame(frame);
        renderer.invalidate();
        // 帧首管线复位，但 textureFactor 保持（真机 TFACTOR 为跨帧持续的
        // 设备态：D3D 初始 0，仅 world_quad 族批设置写入——oracle f500 @R
        // 携带上帧残留 0x2aff0140 实证，见蓝图 §8.8）。
        const auto factor_kept = capture.pipeline().textureFactor;
        capture.pipeline() = touhou::graphics::PipelineState{};
        capture.pipeline().textureFactor = factor_kept;
        th13::ScreenViews loading_views{renderer, loading_environment};
        if (!loading_views.camera(th13::DrawCamera::Fullscreen, false))
            return fail("loading camera unavailable");
        // [boot @R] 装载幕帧发 4 条边条（oracle raw f1 起每帧恒定；raw f0 即
        // prepare 首渲染保持空帧——oracle f0 也无 op，两侧 prefix 均=1 对齐）。
        if (frame != 0) th13::emit_title_side_strips(loading_animations);
        // sig 幕止于 raw f179（oracle 实测：f179 末日、f180 空档帧仅 @R、
        // f181 背景入场——开场计数 0xb4 使能标题任务在 f181）。
        if (frame < 180)
            for (th13::u32 layer = 0; layer < 33; layer++)
                if (!renderer.draw_layer(loading_animations.registry.layer(layer)))
                    return fail("loading layer draw: " + renderer.error);
        renderer.flush();
        capture.clear_device_texture();  // 0x45d2f0:49 帧尾 SetTexture(0,0)（契约 §2.1）
        return capture.end_frame();
    }
    // boot 一帧（ApplicationState::advance_loading(1/60) 镜像：主池→副池各一趟）。
    bool advance_loading(unsigned frame) {
        if (!loading_animations.update(false) || !loading_animations.update(true))
            return fail("loading tick: " + loading_animations.error);
        // [boot tick 对齐] oracle raw f1（截窗 f0）sig alpha=76=n3：装载趟 tick#1
        // + raw f0 update walk tick#2（空帧不画）+ raw f1 tick#3。port 侧
        // prepare render0 即画出（n2），首个 advance 帧补一趟 tick 对齐 n3。
        if (frame == 1 && !loading_animations.update(false))
            return fail("loading tick: " + loading_animations.error);
        ++loading_frames;
        return render_loading_frame(frame);
    }
    // 标题期（ApplicationState::initialize 的 bank 面/回调面 + begin_title(0) 镜像；
    // web 在装载幕完成前即装载完毕 7 bank，本侧在 boot 180 帧后装载，装载期
    // 行为等价——7 bank 装载不触帧序，装载幕期间主管理器不被步进）。
    bool prepare_title() {
        environment.raster_scale = 1;
        environment.render_target_has_alpha = true;
        environment.screen_offsets = {32, 16, 224, 16};
        environment.playfield_origin = {32, 16};
        constexpr std::pair<const char*, th13::i32> banks[7] = {
            {"text.anm", 0},  {"sig.anm", 1},   {"ascii.anm", 2}, {"effect.anm", 8},
            {"title.anm", 16}, {"title_v.anm", 17}, {"help.anm", 19}};
        for (const auto& bank : banks) {
            if (!read_bank(animations, bank.first, bank.second)) return false;
            if (bank.second == 2) environment.fallback_sprite_resource = animations.resource(2);
        }
        // @R 身份自证（比对契约键 (0,2,"@R")/(0,3,"@R")）：text.anm(槽0) entry2/3
        // 为 640×480 离屏渲染目标标记（无纹素，契约只比身份与几何）——解析出的
        // 名即登记身份，read_bank 已随槽 0 全量入表；此处打印供导出报告核对。
        if (const auto* text = animations.resource(0)) {
            if (text->textures.size() < 4) return fail("text.anm @R entries missing");
            std::printf("title rt identities: (0,2,%s) (0,3,%s)\n",
                        text->textures[2].name.c_str(), text->textures[3].name.c_str());
            if (text->textures[2].name != "@R" || text->textures[3].name != "@R")
                return fail("text.anm entry2/3 not @R: contract identity mismatch");
        } else {
            return fail("text.anm bank unavailable");
        }
        animation_updates[0].owner = animation_updates[1].owner = this;
        animation_updates[0].enabled = animation_updates[1].enabled = true;
        animation_updates[0].run = [](void* p) {
            return static_cast<TitleSession*>(p)->animations.update(true)
                       ? th13::i32(th13::FrameAction::Continue)
                       : th13::i32(th13::FrameAction::Error);
        };
        animation_updates[1].run = [](void* p) {
            return static_cast<TitleSession*>(p)->animations.update(false)
                       ? th13::i32(th13::FrameAction::Continue)
                       : th13::i32(th13::FrameAction::Error);
        };
        if (scheduler.add(animation_updates[0], th13::FramePass::Update, 9) < 0 ||
            scheduler.add(animation_updates[1], th13::FramePass::Update, 34) < 0)
            return fail("animation callback registration failed");
        draw_schedule = std::make_unique<th13::AnmDrawSchedule>(scheduler, animations, renderer,
                                                               capture, views);
        if (!draw_schedule->error.empty()) return fail(draw_schedule->error);
        title = std::make_unique<th13::TitleScene>(progress, selection, animations, host,
                                                   scheduler, th13::TitleSceneResources{});
        if (!title->error.empty()) return fail(title->error);
        title->state.return_reason = 0;  // 冷启动 boot 序（begin_title(0)）
        return true;
    }
    // 标题一帧（ApplicationState::step 标题分支 + SceneDisplay::draw 镜像）。
    bool title_step(unsigned frame, th13::u32 held, th13::u32 pressed) {
        title->controls({held, pressed, 0, 0});
        if (scheduler.update() < 0)
            return fail(title->error.empty() ? (title->frame.error.empty() ? animations.error
                                                                           : title->frame.error)
                                             : title->error);
        // [探针] raw 372-374 + 380-381（截窗 371-373 / 379-380）层 26 清单。
        if ((frame >= 372 && frame <= 374) || frame == 380 || frame == 381) {
            std::printf("LAYER26 frame=%u:", frame);
            for (auto* vm : animations.registry.layer(26))
                if (vm) std::printf(" s%d/alpha=%02x/vis=%d", vm->source_script,
                                    vm->visual.color >> 24, int(vm->visible));
            std::printf("\n");
            std::fflush(stdout);
        }
        capture.begin_frame(frame);
        if (scheduler.draw() < 0)
            return fail(!draw_schedule->error.empty() ? draw_schedule->error
                                                      : title->error.empty() ? "title draw failed"
                                                                             : title->error);
        renderer.flush();
        capture.clear_device_texture();
        return capture.end_frame();
    }
    // ~ApplicationState 的摘回调序（title → 动画回调 → display）。
    void teardown() {
        title.reset();
        for (auto& callback : animation_updates) scheduler.remove(callback);
        draw_schedule.reset();
        capture.close_output();
    }
};

// 与 oracle cut_window 同规则的截窗：首个 ops>0 帧重编号 0 起、固定窗长（窗A）；
// 行内 ops 判定走 DrawCapture 固定键序（"ops":[] 即空），帧号改写按同键序就位。
struct TitleWindowCut {
    unsigned prefix = 0, frames = 0, min_ops = 0;
    unsigned long long max_ops = 0, total_ops = 0;
    bool ok = false;
};

unsigned title_line_ops(const std::string& line) {
    unsigned ops = 0;
    for (size_t at = line.find("\"seq\":"); at != std::string::npos; at = line.find("\"seq\":", at + 1))
        ops++;
    return ops;
}

TitleWindowCut cut_title_window(const char* path, unsigned want, bool fixed_window) {
    TitleWindowCut out;
    std::vector<th13::u8> bytes;
    if (!read_whole_file(path, bytes)) return out;
    std::vector<std::string> lines;
    std::string current;
    for (th13::u8 c : bytes) {
        if (c == '\n') {
            if (!current.empty()) lines.push_back(current);
            current.clear();
        } else {
            current.push_back(char(c));
        }
    }
    if (!current.empty()) lines.push_back(current);
    const auto drawn = [](const std::string& line) {
        return line.find("\"ops\":[]") == std::string::npos;
    };
    size_t first = 0;
    while (first < lines.size() && !drawn(lines[first])) first++;
    if (first >= lines.size()) return out;
    size_t end = lines.size();
    if (fixed_window) {
        end = first + want;
        if (end > lines.size()) return out;
    } else {
        while (end > first && !drawn(lines[end - 1])) end--;  // 尾部 ops=0 回Trim（oracle 同则）
    }
    std::FILE* file = std::fopen(path, "wb");
    if (!file) return out;
    unsigned index = 0;
    for (size_t i = first; i < end; i++) {
        std::string& line = lines[i];
        const size_t at = line.find("\"frame\":");
        const size_t digits = line.find(',', at + 8);
        if (at == std::string::npos || digits == std::string::npos) {
            std::fclose(file);
            return out;
        }
        line.replace(at + 8, digits - (at + 8), std::to_string(index));
        const unsigned ops = title_line_ops(line);
        if (index == 0 || ops < out.min_ops) out.min_ops = ops;
        if (ops > out.max_ops) out.max_ops = ops;
        out.total_ops += ops;
        line.push_back('\n');
        std::fwrite(line.data(), 1, line.size(), file);
        index++;
    }
    std::fclose(file);
    out.prefix = unsigned(first);
    out.frames = index;
    out.ok = index > 0;
    return out;
}

// 两窗输入时刻表（真机全局帧 → controls u32 单帧置位；位布局 oracle 实测：
// Z=0x01 ↑=0x10 ↓=0x20 ←=0x40 →=0x80）。port 全局帧 = 渲染序号，与 oracle
// f0（开场创建当帧）1:1 对齐，注入帧号同刻表。
std::map<unsigned, th13::u32> title_input_script(bool window_b) {
    if (window_b)
        return {{470, 0x01}, {530, 0x20}, {590, 0x01}, {650, 0x80}, {710, 0x01}};
    return {{380, 0x20}, {420, 0x10}, {470, 0x01}};
}

int run_title(const char* resource_dir, const char* draws_path, bool window_b) {
    std::printf("title mode start dir=%s out=%s window=%c\n", resource_dir, draws_path,
                window_b ? 'b' : 'a');
    std::fflush(stdout);
    constexpr unsigned kBootFrames = 180;  // boot 装载幕（oracle g181 = 标题使能）
    // 堆置会话（双 AnmManager ≈128KB > wasm 栈；见 TitleSession 注）。
    const std::unique_ptr<TitleSession> session(new TitleSession());
    session->dir = resource_dir;
    th13::draw_capture = &session->capture;
    if (!session->capture.open_output(draws_path)) {
        std::printf("title output failed: %s\n", session->capture.error().c_str());
        return 1;
    }
    if (!session->prepare_loading()) {
        std::printf("title boot failed: %s\n", session->error.c_str());
        return 1;
    }
    std::printf("title boot prepared sig=%u\n", session->loading_signature);
    std::fflush(stdout);
    for (unsigned frame = 1; frame <= kBootFrames; frame++) {
        if (!session->advance_loading(frame)) {
            std::printf("title boot failed at frame %u: %s\n", frame, session->error.c_str());
            return 1;
        }
        if (frame % 60 == 0) {
            std::printf("title boot frame=%u ops=%llu\n", frame,
                        static_cast<unsigned long long>(session->capture.quads() +
                                                        session->capture.ups()));
            std::fflush(stdout);
        }
    }
    if (!session->prepare_title()) {
        std::printf("title load failed: %s\n", session->error.c_str());
        return 1;
    }
    std::printf("title scene ready\n");
    std::fflush(stdout);
    const std::map<unsigned, th13::u32> script = title_input_script(window_b);
    // 窗A：多跑 10 帧余量（prefix 0..10 内截 600 帧不短窗）；窗B：跑到开局
    // 转场请求帧（含该帧），900 帧兜底（到不了 = 如实报错）。
    const unsigned first_step = kBootFrames + 1;
    const unsigned cap = window_b ? 900 : first_step + 429;
    unsigned transition_frame = 0;
    for (unsigned frame = first_step; frame < cap; frame++) {
        const auto at = script.find(frame);
        const th13::u32 bits = at == script.end() ? 0 : at->second;
        if (!session->title_step(frame, bits, bits)) {
            std::printf("title error at frame %u: %s\n", frame, session->error.c_str());
            return 1;
        }
        if (window_b && session->host.run_requested) {
            transition_frame = frame;
            break;
        }
        if (frame % 60 == 0) {
            std::printf("title frame=%u screen=%d substate=%d ops=%llu\n", frame,
                        i32(session->title->state.screen), session->title->state.substate,
                        static_cast<unsigned long long>(session->capture.quads() +
                                                        session->capture.ups()));
            std::fflush(stdout);
        }
    }
    if (window_b && transition_frame == 0) {
        std::printf("title-b: no game start request within %u frames\n", cap);
        return 1;
    }
    const unsigned long long quads = session->capture.quads(), ups = session->capture.ups();
    const unsigned long long raw_frames = session->capture.frames_out();
    session->teardown();
    const TitleWindowCut cut =
        cut_title_window(draws_path, window_b ? 0 : 600, !window_b);
    if (!cut.ok) {
        std::printf("title window cut failed (raw frames=%llu)\n", raw_frames);
        return 1;
    }
    std::printf("port-title-draws window=%c prefix=%u frames=%u min_ops=%u max_ops=%llu "
                "total_ops=%llu transition=%u stage=%d quads=%llu ups=%llu raw=%llu\n",
                window_b ? 'b' : 'a', cut.prefix, cut.frames, cut.min_ops, cut.max_ops,
                cut.total_ops, transition_frame, session->host.requested_stage, quads, ups,
                raw_frames);
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
    if (argc >= 4 && (std::strcmp(argv[1], "title") == 0 || std::strcmp(argv[1], "title-b") == 0))
        // title/title-b <资源目录> <绘制输出>（窗长内置：A=600、B=转场请求帧）
        return run_title(argv[2], argv[3], std::strcmp(argv[1], "title-b") == 0);
    return rng_smoke();
}
