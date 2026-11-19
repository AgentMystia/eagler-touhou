#include "ScoreFile.hpp"

#include "Lzss.hpp"
#include "ResourceCrypt.hpp"
#include "SpellPracticeTable.hpp"

namespace th16 {

namespace {

void put_u32(u8* p, u32 v) { std::memcpy(p, &v, 4); }
void put_u16(u8* p, u16 v) { std::memcpy(p, &v, 2); }

// 0x449880 失败分支：新建 0x18 字节头（魔数、ver 2、+0xc = 0x100，其余 0）
void fresh_header(u8 (&header)[ScoreFile::kFileHeaderSize]) {
    std::memset(header, 0, sizeof(header));
    put_u32(header, ScoreFile::kFileMagic);
    put_u16(header + 8, 2);
    put_u32(header + 0xc, 0x100);
}

// 0x4493c0
void reset_block(ScoreBlock& b) {
    b = ScoreBlock{};
    b.magic = u16(ScoreBlock::kMagic);
    b.version = 1;
    b.size = ScoreBlock::kSize;
    for (auto& difficulty : b.ranks) {
        for (int i = 0; i < ScoreBlock::kRanks; ++i) {
            ScoreEntry& e = difficulty[i];
            e.score = u32(100000 - 10000 * i);
            e.stage = 1;
            std::memcpy(e.name, "--------", 9);
        }
    }
    for (int i = 0; i < ScoreBlock::kSpellRecords; ++i) {
        b.spells[i].id = i;
        b.spells[i].difficulty = i32(SpellPracticeTable::kSpellDifficulty[i]);
    }
}

// 0x449720（noise 留 0，由调用方按全局 RNG 填）
void reset_stats(ScoreStats& s) {
    s = ScoreStats{};
    s.magic = u16(ScoreStats::kMagic);
    s.version = 1;
    s.size = ScoreStats::kSize;
    std::memcpy(s.last_name, ScoreFile::kDefaultName, 9);
}

}  // namespace

void ScoreFile::reset() {
    reset_stats(stats);
    for (ScoreBlock& b : blocks) reset_block(b);
    fresh_header(file_header);
}

u32 ScoreFile::block_checksum(const void* block, u32 size) {
    const u8* p = static_cast<const u8*>(block);
    u32 sum = 0;
    for (u32 i = 8; i < size; ++i) sum += p[i];
    return sum;
}

bool ScoreFile::load_image(const u8* bytes, u32 size) {
    if (bytes == nullptr || size < kFileHeaderSize || read_u32(bytes) != kFileMagic || read_u16(bytes + 8) != 2) {
        fresh_header(file_header);
        return false;
    }
    std::memcpy(file_header, bytes, kFileHeaderSize);
    const u32 packed = read_u32(bytes + 0x10);
    const u32 unpacked = read_u32(bytes + 0x14);
    if (packed > size - kFileHeaderSize) return true;

    std::vector<u8> plain(packed);
    const CryptParams crypt{kCryptKey, kCryptStep, kCryptBlock, packed};
    if (packed != 0 && !resource_crypt(bytes + kFileHeaderSize, plain.data(), packed, crypt, false)) return true;
    std::vector<u8> payload(unpacked);
    u32 written = 0;
    Lzss lzss;
    if (unpacked != 0) lzss.decode(plain.data(), packed, payload.data(), unpacked, written);

    i32 remaining = i32(unpacked);
    u32 at = 0;
    while (remaining >= 1 && at + 0xc <= unpacked) {
        const u8* chunk = payload.data() + at;
        const u16 magic = read_u16(chunk);
        const u16 version = read_u16(chunk + 2);
        const u32 chunk_size = read_u32(chunk + 8);
        if (magic == ScoreBlock::kMagic) {
            if (version == 1 && chunk_size == ScoreBlock::kSize && at + ScoreBlock::kSize <= unpacked &&
                block_checksum(chunk, ScoreBlock::kSize) == read_u32(chunk + 4)) {
                // 原版不检查序号范围（越界即写坏内存）；宿主只接受 0..4
                const u32 index = read_u32(chunk + 0xc);
                if (index < u32(kShots)) std::memcpy(&blocks[index], chunk, ScoreBlock::kSize);
            }
        } else if (magic == ScoreStats::kMagic) {
            if (version == 1 && chunk_size == ScoreStats::kSize && at + ScoreStats::kSize <= unpacked &&
                block_checksum(chunk, ScoreStats::kSize) == read_u32(chunk + 4)) {
                std::memcpy(&stats, chunk, ScoreStats::kSize);
            }
        } else {
            break;
        }
        remaining -= i32(chunk_size);
        if (remaining < 0 || chunk_size == 0) break;
        at += chunk_size;
    }
    return true;
}

std::vector<u8> ScoreFile::save_image() {
    std::vector<u8> payload;
    payload.reserve(kShots * ScoreBlock::kSize + ScoreStats::kSize);
    for (u32 shot = 0; shot < u32(kShots); ++shot) {
        ScoreBlock& b = blocks[shot];
        if (b.magic != ScoreBlock::kMagic) continue;
        b.index = shot;
        b.checksum = block_checksum(&b, ScoreBlock::kSize);
        const u8* raw = reinterpret_cast<const u8*>(&b);
        payload.insert(payload.end(), raw, raw + ScoreBlock::kSize);
    }
    stats.checksum = block_checksum(&stats, ScoreStats::kSize);
    const u8* raw_stats = reinterpret_cast<const u8*>(&stats);
    payload.insert(payload.end(), raw_stats, raw_stats + ScoreStats::kSize);

    Lzss lzss;
    const std::vector<u8> packed_plain = lzss.encode(payload.data(), u32(payload.size()));
    const u32 packed = u32(packed_plain.size());
    put_u32(file_header + 0x14, u32(payload.size()));
    put_u32(file_header + 0x10, packed);
    put_u32(file_header + 4, packed + kFileHeaderSize);

    std::vector<u8> out(kFileHeaderSize + packed);
    std::memcpy(out.data(), file_header, kFileHeaderSize);
    const CryptParams crypt{kCryptKey, kCryptStep, kCryptBlock, packed};
    if (packed != 0) resource_crypt(packed_plain.data(), out.data() + kFileHeaderSize, packed, crypt, true);
    return out;
}

}  // namespace th16
