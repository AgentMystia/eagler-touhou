// th16 无头 harness：不依赖 SDL/GL，驱动 game/ 内核做确定性检查。
// 同一源码编译为 WASI（portable/build-harness.mjs）或本机可执行（tests/run.sh）。
//
//   harness rng <seed> <steps>                 → 每行 "step32 seed calls"（与 golden/rng.json 对照）
//   harness rng-float <seed> <steps>           → 每行 "unit signed angle"（IEEE 位型，十进制无符号）
//   harness archive-list <th16.dat>            → 每行 "name size fnv1a64"（与 thtk 解包对照）
//   harness archive-extract <th16.dat> <name> <out>
//   harness anm <th16.dat> <name.anm>          → 逐 entry 报告：name w h format sprites scripts texture
#include "../../web/cpp/game/Archive.hpp"
#include "../../web/cpp/game/AnmResource.hpp"
#include "../../web/cpp/game/Rng.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace th16;

namespace {

bool read_file(const char* path, std::vector<u8>& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(size_t(size));
    const size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return got == out.size();
}

u64 fnv1a64(const u8* p, size_t n) {
    u64 h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ull; }
    return h;
}

u32 bits(float f) { u32 u; std::memcpy(&u, &f, 4); return u; }

int fail(const char* message) { std::fprintf(stderr, "harness: %s\n", message); return 1; }

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return fail("usage: harness <command> ...");
    const std::string command = argv[1];
    if (command == "rng" && argc >= 4) {
        Rng rng{u16(std::strtoul(argv[2], nullptr, 0))};
        const int steps = std::atoi(argv[3]);
        for (int i = 0; i < steps; ++i) {
            const u32 v = rng.next32();
            std::printf("%u %u %u\n", v, rng.seed, rng.calls);
        }
        return 0;
    }
    if (command == "rng16" && argc >= 4) {
        Rng rng{u16(std::strtoul(argv[2], nullptr, 0))};
        const int steps = std::atoi(argv[3]);
        for (int i = 0; i < steps; ++i) {
            const u16 v = rng.next16();
            std::printf("%u %u %u\n", v, rng.seed, rng.calls);
        }
        return 0;
    }
    if (command == "rng-float" && argc >= 4) {
        const u16 seed = u16(std::strtoul(argv[2], nullptr, 0));
        const int steps = std::atoi(argv[3]);
        Rng a{seed}, b{seed}, c{seed};
        for (int i = 0; i < steps; ++i)
            std::printf("%u %u %u\n", bits(a.unit()), bits(b.signed_unit()), bits(c.angle()));
        return 0;
    }
    std::vector<u8> dat;
    if (argc < 3 || !read_file(argv[2], dat)) return fail("cannot read archive");
    Archive archive;
    if (!archive.open(dat.data(), u32(dat.size()))) return fail(archive.error.c_str());
    if (command == "archive-list") {
        std::vector<u8> bytes;
        for (u32 i = 0; i < archive.entries.size(); ++i) {
            if (!archive.read(i, bytes)) return fail(archive.error.c_str());
            std::printf("%s %zu %016llx\n", archive.entries[i].name.c_str(), bytes.size(), (unsigned long long)fnv1a64(bytes.data(), bytes.size()));
        }
        return 0;
    }
    if (command == "archive-extract" && argc >= 5) {
        std::vector<u8> bytes;
        if (!archive.read(argv[3], bytes)) return fail(archive.error.c_str());
        FILE* f = std::fopen(argv[4], "wb");
        if (!f) return fail("cannot write output");
        std::fwrite(bytes.data(), 1, bytes.size(), f);
        std::fclose(f);
        return 0;
    }
    if (command == "anm" && argc >= 4) {
        std::vector<u8> bytes;
        if (!archive.read(argv[3], bytes)) return fail(archive.error.c_str());
        AnmResource anm;
        if (!anm.parse(bytes.data(), u32(bytes.size()), argv[3])) return fail(anm.error.c_str());
        for (const auto& e : anm.entries) {
            std::printf("%s %u %u %u %zu %zu", e.name.c_str(), e.width, e.height, e.format, e.sprites.size(), e.scripts.size());
            if (e.texture.present())
                std::printf(" thtx:%u:%ux%u:%016llx", e.texture.format, e.texture.width, e.texture.height,
                            (unsigned long long)fnv1a64(e.texture.bgra.data(), e.texture.bgra.size()));
            else std::printf(" thtx:none");
            std::printf("\n");
            for (const auto& s : e.scripts) {
                u32 instructions = 0, sentinel = 0;
                for (const auto& ins : s.instructions) { ++instructions; if (ins.type == 0xffff) ++sentinel; }
                std::printf("  script %d @%u bytes=%zu ins=%u sentinel=%u\n", s.id, s.offset, s.bytes.size(), instructions, sentinel);
            }
        }
        return 0;
    }
    if (command == "anm-texture" && argc >= 6) {
        // anm-texture <th16.dat> <name.anm> <entry-index> <out.bgra>：解码后的 BGRA8 像素（与 thanm -x 的 PNG 对照）
        std::vector<u8> bytes;
        if (!archive.read(argv[3], bytes)) return fail(archive.error.c_str());
        AnmResource anm;
        if (!anm.parse(bytes.data(), u32(bytes.size()), argv[3])) return fail(anm.error.c_str());
        const size_t index = size_t(std::atoi(argv[4]));
        if (index >= anm.entries.size() || !anm.entries[index].texture.present()) return fail("entry has no texture");
        const AnmTexture& t = anm.entries[index].texture;
        FILE* f = std::fopen(argv[5], "wb");
        if (!f) return fail("cannot write output");
        std::fwrite(t.bgra.data(), 1, t.bgra.size(), f);
        std::fclose(f);
        std::printf("%u %u %u %u %u\n", t.format, t.width, t.height, anm.entries[index].x, anm.entries[index].y);
        return 0;
    }
    return fail("unknown command");
}
