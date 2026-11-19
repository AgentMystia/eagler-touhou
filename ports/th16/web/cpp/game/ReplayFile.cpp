#include "ReplayFile.hpp"

#include "Lzss.hpp"
#include "ResourceCrypt.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace th16 {

bool ReplayFile::open_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "open failed";
        return false;
    }
    std::vector<u8> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return open(bytes.data(), u32(bytes.size()));
}

bool ReplayFile::open(const u8* bytes, u32 size) {
    error.clear();
    user.clear();
    stages.clear();
    stage_count = 0;
    std::memset(name, 0, sizeof(name));
    if (bytes == nullptr || size < kHeaderSize) {
        error = "too short";
        return false;
    }
    if (read_u32(bytes) != kMagic) {
        error = "bad magic";
        return false;
    }
    if (read_u16(bytes + 4) != kVersion) {
        error = "bad version";
        return false;
    }
    const u32 packed = read_u32(bytes + 0x1c);
    const u32 unpacked = read_u32(bytes + 0x20);
    if (packed == 0 || unpacked == 0 || size < kHeaderSize + packed) {
        error = "bad sizes";
        return false;
    }

    std::vector<u8> pass1(packed), pass2(packed);
    const CryptParams crypt_a{0x5c, u8(i8(-0x1f)), 0x400, packed};
    const CryptParams crypt_b{0x7d, u8(':'), 0x100, packed};
    if (!resource_crypt(bytes + kHeaderSize, pass1.data(), packed, crypt_a, false) ||
        !resource_crypt(pass1.data(), pass2.data(), packed, crypt_b, false)) {
        error = "crypt failed";
        return false;
    }

    std::vector<u8> decoded(unpacked);
    u32 written = 0;
    Lzss lzss;
    if (!lzss.decode(pass2.data(), packed, decoded.data(), unpacked, written) || written != unpacked) {
        error = "lzss failed";
        return false;
    }
    if (written < kUserSize) {
        error = "userdata truncated";
        return false;
    }

    user.assign(decoded.begin(), decoded.begin() + kUserSize);
    std::memcpy(name, user.data(), 8);
    name[8] = '\0';
    stage_count = read_i32(user.data() + 0x80);
    if (stage_count < 0) stage_count = 0;
    if (stage_count > 7) stage_count = 7;  // 0x448c10 上限按 6 段枚举，但 +0x80 原样保留

    u32 offset = kUserSize;
    const i32 limit = stage_count > 7 ? 6 : stage_count;
    for (i32 i = 0; i < limit; ++i) {
        if (offset + kStageHeaderSize > written) {
            error = "stage header truncated";
            return false;
        }
        ReplayStageSegment seg;
        seg.stage = i16(read_u16(decoded.data() + offset));
        seg.rng_seed = read_u16(decoded.data() + offset + 2);
        seg.frames = read_i32(decoded.data() + offset + 4);
        const i32 extra = read_i32(decoded.data() + offset + 8);
        if (seg.frames < 0 || extra < 0) {
            error = "bad stage sizes";
            return false;
        }
        const u32 body = u32(extra);
        if (offset + kStageHeaderSize + body > written) {
            error = "stage body truncated";
            return false;
        }
        const u32 input_bytes = u32(seg.frames) * 6u;
        const u32 fps_bytes = body >= input_bytes ? body - input_bytes : 0;
        const u8* body_ptr = decoded.data() + offset + kStageHeaderSize;
        seg.inputs.assign(body_ptr, body_ptr + input_bytes);
        seg.fps.assign(body_ptr + input_bytes, body_ptr + input_bytes + fps_bytes);
        stages.push_back(std::move(seg));
        offset += kStageHeaderSize + body;
    }
    return true;
}

bool replay_playback_step(const ReplayFile& file, ReplayPlaybackCursor& cursor,
                          u32& held_low16, u32& aux0, u32& aux1, u8& fps_byte) {
    held_low16 = 0;
    aux0 = 0;
    aux1 = 0;
    if (cursor.ended || cursor.stage_index < 0 ||
        cursor.stage_index >= i32(file.stages.size())) {
        cursor.ended = true;
        fps_byte = cursor.fps_byte;
        return false;
    }
    const ReplayStageSegment& seg = file.stages[size_t(cursor.stage_index)];
    if (cursor.frame_index < 0 || cursor.frame_index >= seg.frames) {
        cursor.ended = true;
        fps_byte = cursor.fps_byte;
        return false;
    }
    const u32 at = u32(cursor.frame_index) * 6u;
    if (at + 6 <= seg.inputs.size()) {
        held_low16 = read_u16(seg.inputs.data() + at);
        aux0 = read_u16(seg.inputs.data() + at + 2);
        aux1 = read_u16(seg.inputs.data() + at + 4);
    }
    // 0x448130：每 30 帧从 FPS 流取一字节（计数为段内帧；第 0/30/… 帧）
    if ((cursor.frame_index % 30) == 0) {
        const u32 fps_i = u32(cursor.frame_index / 30);
        if (fps_i < seg.fps.size()) cursor.fps_byte = seg.fps[fps_i];
    }
    fps_byte = cursor.fps_byte;
    ++cursor.frame_index;
    return true;
}

}  // namespace th16
