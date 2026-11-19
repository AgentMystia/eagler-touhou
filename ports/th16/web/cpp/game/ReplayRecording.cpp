#include "ReplayRecording.hpp"

#include "GameGlobals.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sys/stat.h>

namespace th16 {
namespace {

void put_i32(u8* p, i32 v) { std::memcpy(p, &v, 4); }
void put_u32(u8* p, u32 v) { std::memcpy(p, &v, 4); }
void put_f32(u8* p, float v) { std::memcpy(p, &v, 4); }
i32 load_i32(const u8* p) {
    i32 v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

void append_user_block(std::vector<u8>& out, u8 kind, const std::string& text) {
    const u32 size = (12u + u32(text.size()) + 1u + 3u) & ~3u;
    const u32 start = u32(out.size());
    out.resize(start + size, 0);
    put_u32(out.data() + start, 0x52455355u);  // "USER"
    put_u32(out.data() + start + 4, size);
    out[start + 8] = kind;
    std::memcpy(out.data() + start + 12, text.data(), text.size());
}

bool ensure_replay_dir(const char* directory) {
    char path[512];
    std::snprintf(path, sizeof(path), "%s/replay", directory);
#if defined(_WIN32)
    _mkdir(path);
#else
    ::mkdir(path, 0755);
#endif
    return true;
}

}  // namespace

void ReplayRecording::reset() {
    user.assign(ReplayFile::kUserSize, 0);
    stages.clear();
    current = {};
    stage_open = false;
    sealed = false;
    timestamp = 0;
    error.clear();
}

void ReplayRecording::begin_run(const GameGlobals& globals, i32 time32) {
    reset();
    timestamp = time32;
    put_i32(user.data() + 0xc, time32);
    put_i32(user.data() + 0x84, globals.shot);
    put_i32(user.data() + 0x88, globals.shot_aux);
    put_i32(user.data() + 0x8c, globals.difficulty);
    put_i32(user.data() + 0x90, globals.stage);
    put_i32(user.data() + 0x98, globals.spell_id);
    put_i32(user.data() + 0x9c, globals.season);
    // +0xa bit1：符卡练习
    if ((globals.flags & GameGlobals::kModeMask) == 0x20) user[0xa] = u8(user[0xa] | 2);
}

void ReplayRecording::begin_stage(i16 stage, u16 rng_seed) {
    if (sealed) return;
    if (stage_open) close_stage(false);
    current = {};
    current.stage = stage;
    current.rng_seed = rng_seed;
    current.frames = 0;
    stage_open = true;
}

bool ReplayRecording::tick(u16 held, u16 pressed, u16 released, u8 fps_byte) {
    if (sealed || !stage_open) return true;
    if (current.inputs.size() + 6 > kMaxInputBytes) {
        error = "replay input overflow";
        return false;
    }
    if ((current.frames % 30) == 0) current.fps.push_back(fps_byte);
    u8 frame[6];
    std::memcpy(frame + 0, &held, 2);
    std::memcpy(frame + 2, &pressed, 2);
    std::memcpy(frame + 4, &released, 2);
    current.inputs.insert(current.inputs.end(), frame, frame + 6);
    ++current.frames;
    return true;
}

void ReplayRecording::close_stage(bool append_terminator) {
    if (!stage_open) return;
    if (append_terminator) {
        const u16 term = 0xffff;
        u8 frame[6];
        std::memcpy(frame + 0, &term, 2);
        std::memcpy(frame + 2, &term, 2);
        std::memcpy(frame + 4, &term, 2);
        current.inputs.insert(current.inputs.end(), frame, frame + 6);
        ++current.frames;
        // 终止帧也占 FPS 槽：仅当 frames%30==0 时原版在写入前已采样；此处追加后对齐帧数即可
    }
    stages.push_back(std::move(current));
    current = {};
    stage_open = false;
}

bool ReplayRecording::encode(std::vector<u8>& out, const char* player_name, i32 score, float slow_rate) {
    if (!error.empty()) return false;
    if (stage_open) close_stage(true);
    if (stages.empty()) {
        error = "no stages";
        return false;
    }
    if (player_name == nullptr) player_name = "";
    char name8[9] = {};
    std::memset(name8, ' ', 8);
    const size_t n = std::strlen(player_name);
    std::memcpy(name8, player_name, n > 8 ? 8 : n);

    ReplayFile file;
    file.user = user;
    if (file.user.size() != ReplayFile::kUserSize) file.user.assign(ReplayFile::kUserSize, 0);
    std::memcpy(file.user.data(), name8, 8);
    put_i32(file.user.data() + 0xc, timestamp);
    put_i32(file.user.data() + 0x14, score);
    put_f32(file.user.data() + 0x7c, slow_rate);
    put_i32(file.user.data() + 0x80, i32(stages.size()));
    file.stages = stages;
    file.stage_count = i32(stages.size());
    std::memcpy(file.name, name8, 8);

    std::vector<u8> body;
    if (!file.encode(body)) {
        error = file.error.empty() ? "encode failed" : file.error;
        return false;
    }

    // 尾部 USER 说明块（0x448400）：kind0 文本摘要 + kind1 日文提示；解码侧忽略
    char info[512];
    std::snprintf(info, sizeof(info),
                  "Touhou 16 replay\r\nVersion 1.00a\r\nName %.8s\r\nScore %d\r\nSlow Rate %2.2f\r\n",
                  name8, score, double(slow_rate));
    append_user_block(body, 0, info);
    append_user_block(body, 1, "Comment placeholder");
    out = std::move(body);
    sealed = true;
    return true;
}

bool ReplayRecording::write_slot(const char* directory, int slot, const char* player_name, i32 score,
                                 float slow_rate) {
    if (directory == nullptr || slot < 1 || slot > 100) {
        error = "bad slot";
        return false;
    }
    std::vector<u8> bytes;
    if (!encode(bytes, player_name, score, slow_rate)) return false;
    ensure_replay_dir(directory);
    char path[512];
    std::snprintf(path, sizeof(path), "%s/replay/th16_%02d.rpy", directory, slot);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::snprintf(path, sizeof(path), "%s/th16_%02d.rpy", directory, slot);
        out.open(path, std::ios::binary);
    }
    if (!out) {
        error = "open for write failed";
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out) {
        error = "write failed";
        return false;
    }
    return true;
}

bool ReplayRecording::slot_exists(const char* directory, int slot) {
    return replay_slot_exists(directory, slot);
}

bool ReplayRecording::slot_summary(const char* directory, int slot, ReplaySummary& out) {
    char path[512];
    if (!replay_slot_path(directory, slot, path, sizeof(path))) return false;
    ReplayFile file;
    if (!file.open_file(path) || file.user.size() < 0xa0) return false;
    std::memset(out.name, ' ', 8);
    out.name[8] = '\0';
    std::memcpy(out.name, file.user.data(), 8);
    out.timestamp = load_i32(file.user.data() + 0xc);
    out.spell_practice = (file.user[0xa] & 2) != 0;
    out.shot = load_i32(file.user.data() + 0x84);
    out.shot_aux = load_i32(file.user.data() + 0x88);
    out.difficulty = load_i32(file.user.data() + 0x8c);
    out.stage = load_i32(file.user.data() + 0x90);
    out.spell_id = load_i32(file.user.data() + 0x98);
    out.season = load_i32(file.user.data() + 0x9c);
    return true;
}

}  // namespace th16
