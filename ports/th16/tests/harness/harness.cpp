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
//   harness scene-boot <th16.dat> <frames> [skip] [f:bits,...]
//                                              → GameSession（与浏览器同一逻辑层）：Boot→Title→主菜单→难度/机体/季节/关卡；frames 帧后报告
//                                                scene/boot_frames/title_live/screen/sub/cursor/difficulty/shot/season/stage/mode/quit/链数/quads，
//                                                随后 "event sfx|screen|scene|bgm-fade|transition|stub N" 与各四边形。skip = DAT_004c1800&0x180；
//                                                f:bits 在第 f 帧注入 held 位（0x1 Z，0x2 X，0x10 ↑，0x20 ↓，0x100 Esc，0x80000 Enter）
//   harness chain-demo                          → ChainManager 走法自检
//   harness frame-clock <t0> <t1> ...           → 每个墙钟时刻应执行的逻辑步数
//   harness input-edges <held0> <held1> ...     → 0x405580 边沿/连发/释放位
#include "../../web/cpp/game/Archive.hpp"
#include "../../web/cpp/game/AnmManager.hpp"
#include "../../web/cpp/game/AnmRenderer.hpp"
#include "../../web/cpp/game/AnmResource.hpp"
#include "../../web/cpp/game/Chain.hpp"
#include "../../web/cpp/game/FrameClock.hpp"
#include "../../web/cpp/game/GameSession.hpp"
#include "../../web/cpp/game/Input.hpp"
#include "../../web/cpp/game/Rng.hpp"
#include "../../web/cpp/game/Std.hpp"
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
    if (command == "chain-demo") {
        // ChainManager 语义自检：优先级排序、启用位、Remove/Restart 返回码（0x401460 走法）
        ChainManager chains;
        std::string trace;
        int restarts = 0;
        ChainEntry* a = chains.create([&]() { trace += "A"; return ChainResult::Continue; }, 5, true);
        ChainEntry* b = chains.create([&]() { trace += "B"; return ChainResult::Remove; }, 1, true);
        ChainEntry* c = chains.create([&]() {
            trace += "C";
            return restarts++ == 0 ? ChainResult::Restart : ChainResult::Continue;
        }, 3, true);
        ChainEntry* d = chains.create([&]() { trace += "D"; return ChainResult::Continue; }, 7, true);
        chains.enable(a); chains.enable(b); chains.enable(c);  // d 未启用
        const i32 r1 = chains.run_update();
        trace += "|";
        const i32 r2 = chains.run_update();
        chains.enable(d);
        chains.disable(a);
        trace += "|";
        const i32 r3 = chains.run_update();
        std::printf("%s r=%d,%d,%d count=%u\n", trace.c_str(), r1, r2, r3, chains.update_count());
        return 0;
    }
    if (command == "frame-clock") {
        // FrameClock：按给定墙钟序列（秒）输出每次应执行的逻辑步数
        FrameClock clock;
        std::string out;
        for (int a = 2; a < argc; ++a) {
            const u32 steps = clock.begin_frame(std::atof(argv[a]));
            out += std::to_string(steps);
            if (a + 1 < argc) out += ",";
        }
        std::printf("%s\n", out.c_str());
        return 0;
    }
    if (command == "input-edges") {
        // 0x405580：held 序列 → 每帧 "pressed repeat released"
        InputState in;
        for (int a = 2; a < argc; ++a) {
            in.held = u32(std::strtoul(argv[a], nullptr, 0));
            in.commit();
            std::printf("%x %x %x\n", in.pressed, in.repeat, in.released);
        }
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
    if (command == "std" && argc >= 4) {
        // STD 解析（0x40ac30）：头 / 对象 / 四边形 / 实例 / 脚本
        std::vector<u8> bytes;
        if (!archive.read(argv[3], bytes)) return fail(archive.error.c_str());
        StdFile std_file;
        if (!std_file.parse(bytes.data(), u32(bytes.size()))) return fail(std_file.error.c_str());
        std::printf("anm=%s faces=%d objects=%zu instances=%zu script=%zu\n", std_file.anm_name.c_str(), std_file.face_vm_count,
                    std_file.objects.size(), std_file.instances.size(), std_file.script.size());
        for (const StdObject& o : std_file.objects) {
            std::printf("object %u layer=%u pos=%.6g,%.6g,%.6g size=%.6g,%.6g,%.6g quads=%zu\n", o.id, o.layer,
                        o.pos.x, o.pos.y, o.pos.z, o.size.x, o.size.y, o.size.z, o.quads.size());
            for (const StdQuad& q : o.quads)
                std::printf("  quad type=%d script=%d pos=%.6g,%.6g,%.6g wh=%.6g,%.6g\n", q.type, q.anm_script, q.pos.x, q.pos.y, q.pos.z, q.width, q.height);
        }
        for (const StdInstance& i : std_file.instances)
            std::printf("instance %u flags=%u pos=%.6g,%.6g,%.6g\n", i.object, i.flags, i.pos.x, i.pos.y, i.pos.z);
        for (const StdInstruction& ins : std_file.script) {
            std::printf("ins @%u t=%d op=%d", ins.offset, ins.time, ins.opcode);
            for (int n = 0; n < (ins.size - 8) / 4; ++n) std::printf(" %d/%.6g", ins.arg_i(n), ins.arg_f(n));
            std::printf("\n");
        }
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
        // 与 Application 共用 GameSession：Boot→Title→主菜单。
        // 可选输入脚本："frame:bits,frame:bits,..."（bits 为 InputState 位，十六进制/十进制，持续 1 帧）；
        // "skip" 置 DAT_004c1800 & 0x180 直接放行启动幕等待。
        const int frames = std::atoi(argv[3]);
        GameSession session;
        if (!session.open(dat.data(), u32(dat.size()))) return fail(session.failure.c_str());
        struct Key { int frame; u32 bits; };
        std::vector<Key> script;
        for (int a = 4; a < argc; ++a) {
            if (std::strcmp(argv[a], "skip") == 0) { session.supervisor.config_flags |= Supervisor::kSkipBootFlags; continue; }
            const char* p = argv[a];
            while (*p) {
                char* endp = nullptr;
                const long frame = std::strtol(p, &endp, 10);
                if (endp == p || *endp != ':') return fail("bad input script");
                p = endp + 1;
                const unsigned long b = std::strtoul(p, &endp, 0);
                script.push_back({int(frame), u32(b)});
                p = endp;
                if (*p == ',') ++p;
            }
        }
        if (!session.begin_boot()) return fail(session.failure.c_str());

        AnmRenderer renderer;
        renderer.window_scale = 1.0f;
        renderer.lookup = [](int /*slot*/, int entry) -> u32 { return u32(entry) + 1; };

        for (int f = 0; f < frames; ++f) {
            session.injected_input = 0;
            for (const Key& k : script) if (k.frame == f) session.injected_input |= k.bits;
            if (!session.logic_step()) return fail(session.failure.c_str());
        }
        std::vector<AnmQuad> quads;
        if (!renderer.emit_all(session.animations, quads)) return fail(renderer.error.c_str());
        const Supervisor& sup = session.supervisor;
        const TitleScene& title = session.title;
        const GameGlobals& g = session.globals;
        std::printf("scene=%d boot_frames=%u title_live=%d screen=%d sub=%d cursor=%d difficulty=%d shot=%d season=%d stage=%d mode=%d quit=%d update_chains=%u draw_chains=%u quads=%zu\n",
                    int(sup.current), sup.boot_frames, sup.title_menu_live ? 1 : 0, title.screen, title.sub, title.cursor.index,
                    g.difficulty, g.shot, g.season, g.stage, g.mode(), session.quit_requested ? 1 : 0,
                    session.chains.update_count(), session.chains.draw_count(), quads.size());
        for (const TitleScene::Event& e : session.title_events) std::printf("event %s %d\n", e.kind, e.value);
        for (const TitleScene::Event& e : title.events) std::printf("event %s %d\n", e.kind, e.value);
        for (const AnmQuad& q : quads) {
            std::printf("%d %d %u %.8g %.8g %.8g %.8g %08x z=%.6g,%.6g,%.6g,%.6g rhw=%.6g zw=%d slot=%d\n",
                        q.layer, q.script, q.id, q.x0, q.y0, q.x1, q.y3, q.color0,
                        q.z[0], q.z[1], q.z[2], q.z[3], q.rhw[0], q.depth_write ? 1 : 0, q.file_slot);
        }
        if (session.game_live) {
            // 场景 7：GameManager / StageManager 状态与关卡背景绘制（0x409f90）
            StageManager& st = session.game.stage;
            const StageCamera& c = st.camera;
            StagePass pass;
            if (!renderer.emit_stage(session.animations, st, pass)) return fail(renderer.error.c_str());
            std::printf("game flags=%x frame=%d game_frame=%d stage_frame=%d std=%s anm=%s objects=%zu instances=%zu faces=%d\n",
                        session.game.flags, session.game.timer.current, g.game_frame, g.stage_frame,
                        session.game.entry ? session.game.entry->std_name : "-", st.std.anm_name.c_str(),
                        st.std.objects.size(), st.std.instances.size(), st.std.face_vm_count);
            std::printf("stage frame=%d ins=%d time=%d rocking=%d pos=%.6g,%.6g,%.6g facing=%.6g,%.6g,%.6g up=%.6g,%.6g,%.6g fov=%.8g fog=%08x,%.6g,%.6g bg=%08x\n",
                        st.frame, st.ins_index, st.timer.current, st.rocking_mode,
                        c.pos.x, c.pos.y, c.pos.z, c.facing.x, c.facing.y, c.facing.z, c.up.x, c.up.y, c.up.z, c.fov,
                        c.fog.color, c.fog.near_, c.fog.far_, st.bg_color);
            std::printf("stagepass active=%d clear=%08x quads=%zu drawn_instances=%d drawn_quads=%d\n",
                        pass.active ? 1 : 0, pass.clear_color, pass.quads.size(), st.drawn_instances, st.drawn_quads);
            for (const AnmQuad& q : pass.quads) {
                std::printf("sq %d %d %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %08x z=%.6g,%.6g,%.6g,%.6g rhw=%.6g,%.6g,%.6g,%.6g fog=%d,%u,%u,%u,%u zt=%d zw=%d uv=%.6g,%.6g,%.6g,%.6g\n",
                            q.layer, q.script, q.x0, q.y0, q.x1, q.y1, q.x2, q.y2, q.x3, q.y3, q.color0,
                            q.z[0], q.z[1], q.z[2], q.z[3], q.rhw[0], q.rhw[1], q.rhw[2], q.rhw[3],
                            q.fog ? 1 : 0, q.fog_factor[0], q.fog_factor[1], q.fog_factor[2], q.fog_factor[3],
                            q.depth_test ? 1 : 0, q.depth_write ? 1 : 0, q.u0, q.v0, q.u3, q.v3);
            }
        }
        return 0;
    }
    return fail("unknown command");
}
