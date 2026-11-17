// th16 无头 harness：不依赖 SDL/GL，驱动 game/ 内核做确定性检查。
// 同一源码编译为 WASI（portable/build-harness.mjs）或本机可执行（tests/run.sh）。
//
//   harness rng <seed> <steps>                 → 每行 "step32 seed calls"（与 golden/rng.json 对照）
//   harness rng-float <seed> <steps>           → 每行 "unit signed angle"（IEEE 位型，十进制无符号）
//   harness archive-list <th16.dat>            → 每行 "name size fnv1a64"（与 thtk 解包对照）
//   harness archive-extract <th16.dat> <name> <out>
//   harness anm <th16.dat> <name.anm>          → 逐 entry 报告：name w h format sprites scripts texture
//   harness anm-run <th16.dat> <name.anm> <scripts: all|3|0-20,40> <frames> [layer]
//                                              → JSON：每帧世界链表各 VM 的 {id, script, bytes(0..0x568 hex)} 与 36 条 layer 绘制链
//                                                （与 tools/oracle/anm-oracle.py 直接执行原版得到的快照逐字节对照）
//   harness anm-bench <th16.dat> <name.anm> <script> <frames> [layer]
//                                              → "frames vms fnv1a64"：只推进不输出快照（性能口径）
//   harness anm-draw <th16.dat> <name.anm> <script> <frames> [layer]
//                                              → 每行一层绘制结果："frame layer script id blend x0 y0 x1 y1 x2 y2 x3 y3 u0 v0 u1 v1 u2 v2 u3 v3 color"
//                                                （spawn 后 tick frames 次再 emit；对应上屏几何，texture 句柄以 entry_index+1 占位）
//   harness scene-boot <th16.dat> <frames> [skip_at]
//                                              → 模拟 Supervisor：Boot→Title 装载，frames 帧后报告 scene/boot_frames/title_live/quads
//                                                skip_at≥0 时在该帧注入确定键以提前结束 0xb4 等待
#include "../../web/cpp/game/Archive.hpp"
#include "../../web/cpp/game/AnmManager.hpp"
#include "../../web/cpp/game/AnmRenderer.hpp"
#include "../../web/cpp/game/AnmResource.hpp"
#include "../../web/cpp/game/Rng.hpp"
#include "../../web/cpp/game/Supervisor.hpp"

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
    if (command == "anm-run" && argc >= 6) {
        std::vector<u8> bytes;
        if (!archive.read(argv[3], bytes)) return fail(archive.error.c_str());
        AnmResource anm;
        if (!anm.parse(bytes.data(), u32(bytes.size()), argv[3])) return fail(anm.error.c_str());
        const int frames = std::atoi(argv[5]);
        const int layer = argc >= 7 ? std::atoi(argv[6]) : 0;
        // 原版启动时 AsciiManager（0x407cb0）先把 ascii.anm 装入槽 2；ins_300 负下标回退到它的 0x102 号精灵
        std::vector<u8> ascii_bytes;
        AnmResource ascii;
        if (!archive.read("ascii.anm", ascii_bytes) || !ascii.parse(ascii_bytes.data(), u32(ascii_bytes.size()), "ascii.anm"))
            return fail("cannot load ascii.anm");
        // 脚本选择："all" / "3" / "0-20,40"（与 anm-oracle.py 同语法）
        std::vector<int> scripts;
        {
            const std::string spec = argv[4];
            if (spec == "all") {
                for (int i = 0; i < int(anm.total_scripts); ++i) scripts.push_back(i);
            } else {
                size_t pos = 0;
                while (pos < spec.size()) {
                    size_t comma = spec.find(',', pos);
                    if (comma == std::string::npos) comma = spec.size();
                    const std::string part = spec.substr(pos, comma - pos);
                    const size_t dash = part.find('-');
                    if (dash == std::string::npos) scripts.push_back(std::atoi(part.c_str()));
                    else for (int i = std::atoi(part.substr(0, dash).c_str()); i <= std::atoi(part.substr(dash + 1).c_str()); ++i) scripts.push_back(i);
                    pos = comma + 1;
                }
            }
        }
        // 原版快照只含 0x568 之前的纯数据区（之后为链表指针，宿主地址无意义）
        constexpr size_t kSnapshotBytes = 0x568;
        std::printf("{\"anm\":\"%s\",\"layer\":%d,\"frames\":%d,\"runs\":[", argv[3], layer, frames);
        bool first_run = true;
        for (const int script : scripts) {
            // 每个脚本都从同一装载完成状态开始（oracle 以内存检查点实现；这里重建管理器）
            AnmManager manager;
            manager.load(AnmManager::kAsciiSlot, ascii);
            AnmFile* file = manager.load(0, anm);
            if (file == nullptr || script < 0 || size_t(script) >= file->scripts.size()) return fail("script out of range");
            const u32 spawned = manager.spawn(*file, script, layer);
            auto snapshot = [&](int frame) {
                std::printf("%s{\"frame\":%d,\"vms\":[", frame == 0 ? "" : ",", frame);
                bool first = true;
                for (const AnmListNode* n = manager.world_head(); n != nullptr; n = n->next) {
                    const AnmVm& vm = *n->owner;
                    std::printf("%s{\"id\":%u,\"script\":%d,\"bytes\":\"", first ? "" : ",", vm.id, vm.script_index);
                    const u8* raw = reinterpret_cast<const u8*>(&vm);
                    for (size_t i = 0; i < kSnapshotBytes; ++i) std::printf("%02x", raw[i]);
                    std::printf("\"}");
                    first = false;
                }
                std::printf("],\"layers\":[");
                for (int l = 0; l < AnmManager::kLayerCount; ++l) {
                    std::printf("%s[", l == 0 ? "" : ",");
                    bool first_id = true;
                    for (const AnmVm* vm = manager.layer_chain(l); vm != nullptr; vm = vm->layer_next) {
                        std::printf("%s%u", first_id ? "" : ",", vm->id);
                        first_id = false;
                    }
                    std::printf("]");
                }
                std::printf("]}");
            };
            std::printf("%s{\"script\":%d,\"spawnedId\":%u,\"frames\":[", first_run ? "" : ",", script, spawned);
            first_run = false;
            snapshot(0);
            for (int f = 1; f <= frames; ++f) {
                manager.tick_world();
                snapshot(f);
            }
            std::printf("]}");
        }
        std::printf("]}\n");
        return 0;
    }
    if (command == "anm-draw" && argc >= 6) {
        std::vector<u8> bytes;
        if (!archive.read(argv[3], bytes)) return fail(archive.error.c_str());
        AnmResource anm;
        if (!anm.parse(bytes.data(), u32(bytes.size()), argv[3])) return fail(anm.error.c_str());
        std::vector<u8> ascii_bytes;
        AnmResource ascii;
        if (!archive.read("ascii.anm", ascii_bytes) || !ascii.parse(ascii_bytes.data(), u32(ascii_bytes.size()), "ascii.anm"))
            return fail("cannot load ascii.anm");
        const int script = std::atoi(argv[4]);
        const int frames = std::atoi(argv[5]);
        const int layer = argc >= 7 ? std::atoi(argv[6]) : -1;
        AnmManager manager;
        manager.load(AnmManager::kAsciiSlot, ascii);
        AnmFile* file = manager.load(0, anm);
        if (file == nullptr || script < 0 || size_t(script) >= file->scripts.size()) return fail("script out of range");
        manager.spawn(*file, script, layer);
        AnmRenderer renderer;
        renderer.window_scale = 1.0f;
        renderer.lookup = [](int /*slot*/, int entry) -> u32 { return u32(entry) + 1; };
        for (int f = 0; f <= frames; ++f) {
            manager.tick_world();  // spawn 后每次 tick 建/刷新 layer 链（与 Application::step 同序）
            std::vector<AnmQuad> quads;
            if (!renderer.emit_all(manager, quads)) return fail(renderer.error.c_str());
            for (const AnmQuad& q : quads) {
                std::printf("%d %d %d %u %d %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %08x\n",
                            f, q.layer, q.script, q.id, q.blend,
                            q.x0, q.y0, q.x1, q.y1, q.x2, q.y2, q.x3, q.y3,
                            q.u0, q.v0, q.u1, q.v1, q.u2, q.v2, q.u3, q.v3, q.color0);
            }
        }
        return 0;
    }
    if (command == "anm-bench" && argc >= 6) {
        // anm-bench <th16.dat> <name.anm> <script> <frames> [layer]：只推进不输出快照，末尾给 VM 数与世界链表全部 VM 的 fnv1a64
        std::vector<u8> bytes;
        if (!archive.read(argv[3], bytes)) return fail(archive.error.c_str());
        AnmResource anm;
        if (!anm.parse(bytes.data(), u32(bytes.size()), argv[3])) return fail(anm.error.c_str());
        std::vector<u8> ascii_bytes;
        AnmResource ascii;
        if (!archive.read("ascii.anm", ascii_bytes) || !ascii.parse(ascii_bytes.data(), u32(ascii_bytes.size()), "ascii.anm"))
            return fail("cannot load ascii.anm");
        const int script = std::atoi(argv[4]);
        const int frames = std::atoi(argv[5]);
        const int layer = argc >= 7 ? std::atoi(argv[6]) : 0;
        AnmManager manager;
        manager.load(AnmManager::kAsciiSlot, ascii);
        AnmFile* file = manager.load(0, anm);
        if (file == nullptr || script < 0 || size_t(script) >= file->scripts.size()) return fail("script out of range");
        manager.spawn(*file, script, layer);
        for (int f = 0; f < frames; ++f) manager.tick_world();
        u64 digest = 0xcbf29ce484222325ull;
        u32 count = 0;
        for (const AnmListNode* n = manager.world_head(); n != nullptr; n = n->next) {
            const u8* raw = reinterpret_cast<const u8*>(n->owner);
            for (size_t i = 0; i < 0x568; ++i) { digest ^= raw[i]; digest *= 0x100000001b3ull; }
            ++count;
        }
        std::printf("%d %u %016llx\n", frames, count, (unsigned long long)digest);
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
    if (command == "scene-boot" && argc >= 4) {
        // 无头复现 Application 启动幕→标题时序（不含 SDL 绘制）。
        const int frames = std::atoi(argv[3]);
        const int skip_at = argc >= 5 ? std::atoi(argv[4]) : -1;
        auto load_res = [&](const char* name, AnmResource& out) -> bool {
            std::vector<u8> bytes;
            if (!archive.read(name, bytes) || !out.parse(bytes.data(), u32(bytes.size()), name)) return false;
            return true;
        };
        AnmResource ascii, sig, title, title_v;
        if (!load_res("ascii.anm", ascii) || !load_res("sig.anm", sig) ||
            !load_res("title.anm", title) || !load_res("title_v.anm", title_v))
            return fail("cannot load boot/title anm");
        AnmManager manager;
        Supervisor supervisor;
        manager.load(AnmManager::kAsciiSlot, ascii);
        AnmFile* sig_file = manager.load(1, sig);
        if (sig_file == nullptr) return fail("sig load");
        manager.spawn(*sig_file, 0, -1);
        supervisor.current = SceneId::Boot;
        supervisor.boot_init_finished();

        AnmRenderer renderer;
        renderer.window_scale = 1.0f;
        renderer.lookup = [](int /*slot*/, int entry) -> u32 { return u32(entry) + 1; };

        for (int f = 0; f < frames; ++f) {
            if (skip_at >= 0 && f == skip_at) {
                supervisor.input.held = InputState::kShot;
                supervisor.input.commit();
            } else {
                supervisor.input.held = 0;
                supervisor.input.commit();
            }
            SceneId leave = SceneId::Invalid, enter = SceneId::Invalid;
            if (supervisor.consume_switch(leave, enter) && enter == SceneId::Title) {
                if (manager.load(0x10, title) == nullptr || manager.load(0x11, title_v) == nullptr)
                    return fail("title load");
                supervisor.title_assets_ready = true;
            }
            if (!supervisor.title_menu_live) {
                ++supervisor.boot_frames;
                if (supervisor.title_assets_ready && supervisor.boot_hold_elapsed()) {
                    manager.kill_file(1);
                    manager.tick_world();
                    manager.unload(1);
                    AnmFile* tf = manager.file(0x10);
                    AnmFile* tv = manager.file(0x11);
                    if (tf == nullptr || tv == nullptr) return fail("title banks");
                    manager.spawn(*tf, 0x61, -1);
                    manager.spawn(*tf, 0x65, -1);
                    manager.spawn(*tf, 0, -1);
                    manager.spawn(*tv, 0, -1);
                    supervisor.title_menu_live = true;
                }
            }
            manager.tick_world();
        }
        std::vector<AnmQuad> quads;
        if (!renderer.emit_all(manager, quads)) return fail(renderer.error.c_str());
        std::printf("scene=%d boot_frames=%u title_live=%d quads=%zu\n",
                    int(supervisor.current), supervisor.boot_frames,
                    supervisor.title_menu_live ? 1 : 0, quads.size());
        for (const AnmQuad& q : quads) {
            std::printf("%d %d %u %.8g %.8g %.8g %.8g %08x\n",
                        q.layer, q.script, q.id, q.x0, q.y0, q.x1, q.y3, q.color0);
        }
        return 0;
    }
    return fail("unknown command");
}
