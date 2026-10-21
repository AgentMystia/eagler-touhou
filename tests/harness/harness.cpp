// WASI 无头 harness：同一份 game 源码在 wasm32-wasip1 下独立运行，
// 供 CI 中的 oracle 对照（原版 Unicorn 轨迹 vs C++ 行为）调用。
// 编译方式见 portable/build-harness.mjs（WASI_SDK_PATH 需指向 wasi-sdk）。
// 用法：
//   th13-harness              旧有 RNG 冒烟（8 值输出）
//   th13-harness stage1 <资源目录> <事件输出路径> [帧数] [stage.ecl]
//     资源目录需含 st01.ecl 及其 ECLI/ANIM 依赖（artifacts/reference/raw）。
//   th13-harness replay <rpy 路径> <资源目录> [帧数] [stage 索引]
//     解码原版录像并把输入逐帧注入 StageBattle（0x448200 回放节奏）。
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>
#include <vector>

#include "game/GameInput.hpp"
#include "game/ReplayFile.hpp"
#include "game/Rng.hpp"
#include "game/StageBattle.hpp"
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

int replay(const char* replay_path, const char* resource_dir, unsigned frames, unsigned stage_index) {
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
    std::printf("replay name=%s rank=%d chara=%d score=%d stages=%u\n",
                replay_file.player_name.c_str(), replay_file.rank, replay_file.character,
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
    if (!battle.load(provider, stage_file, 1 << replay_file.rank, host, progress, player,
                     score_state)) {
        std::printf("replay battle load failed: %s\n", battle.error().c_str());
        return 1;
    }

    // 逐帧注入（0x448200：态 → 0x4e4c08，沿由 0x414f30 幂等重算）。
    th13::ReplayInputCursor cursor(stage);
    th13::GameInput input;
    const u32 total = frames ? th13::u32(frames) : stage.frame_count;
    u32 done = 0;
    i32 boss_frame = -1;
    bool boss_seen = false;
    while (done < total && cursor.valid() && !host.game_over) {
        const th13::ReplayFrame frame = cursor.next();
        input.update(frame.state);
        if (!battle.step(input)) {
            std::printf("replay error at frame %u: %s\n", done, battle.error().c_str());
            return 1;
        }
        const bool boss = battle.sim.boss() != nullptr;
        if (boss && !boss_seen) {
            boss_seen = true;
            boss_frame = i32(done);
            std::printf("replay boss frame=%d held=%04x\n", boss_frame, frame.state);
        }
        if (done % 600 == 0)
            std::printf("replay frame=%u held=%04x x=%.1f y=%.1f enemies=%u slow=%u\n", done,
                        frame.state, battle.player_x, battle.player_y,
                        unsigned(battle.sim.enemies.size()), cursor.current_slow_rate());
        ++done;
    }
    std::printf("replay done frames=%u/%u x=%.1f y=%.1f deaths=%d score=%d boss_frame=%d\n",
                done, unsigned(stage.frame_count), battle.player_x, battle.player_y,
                player.deaths, score_state.score, boss_frame);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 4 && std::strcmp(argv[1], "stage1") == 0)
        // stage1 <资源目录> <输出> [frames] [stage.ecl]
        return stage1(argv[2], argv[3], argc >= 5 ? unsigned(std::atoi(argv[4])) : 3600u,
                      argc >= 6 ? argv[5] : nullptr);
    if (argc >= 4 && std::strcmp(argv[1], "replay") == 0)
        // replay <rpy> <资源目录> [frames] [stage 索引]
        return replay(argv[2], argv[3], argc >= 5 ? unsigned(std::atoi(argv[4])) : 0u,
                      argc >= 6 ? unsigned(std::atoi(argv[5])) : 0u);
    return rng_smoke();
}
