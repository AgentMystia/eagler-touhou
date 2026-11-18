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
//   harness std <th16.dat> <stNN.std>          → STD 头 / 对象 / 四边形 / 实例 / 脚本（0x40ac30）
//   harness stage-run <th16.dat> <stage> <frames>    → StageManager 单独驱动（0x409db0→0x40add0→每帧 0x409e50），逐帧 JSON
//   harness stage-digest <th16.dat> <stage> <frames> → 同一状态流的 FNV-1a 64（tests/golden/stage.json，tools/oracle/stage-oracle.py 同口径）
//   harness game-run <th16.dat> <stage> <shot> <season> <frames> <input "f:held,..."> [enemies "f:n,..."] [dialogue "f:v,..."]
//                                              → 直接进入场景 7（GameSession::start_game_direct），逐帧 JSON：Player 规范字段（含射击计时、冷却表、
//                                                自机弹 ×live、伤害源 ×live、本帧 sfx）+ 自机 VM 0x568 字节
//                                                （与 tools/oracle/game-oracle.py 快照同结构；enemies/dialogue 为未移植管理器的 PlayerWorld 桩输入）
//   harness game-digest <...同上...>              → 同一状态流的 FNV-1a 64（tests/golden/player.json，game-oracle.py --digest 同口径）
#include "../../web/cpp/game/Archive.hpp"
#include "../../web/cpp/game/AnmManager.hpp"
#include "../../web/cpp/game/AnmRenderer.hpp"
#include "../../web/cpp/game/AnmResource.hpp"
#include "../../web/cpp/game/Chain.hpp"
#include "../../web/cpp/game/FrameClock.hpp"
#include "../../web/cpp/game/GameSession.hpp"
#include "../../web/cpp/game/Input.hpp"
#include "../../web/cpp/game/Rng.hpp"
#include "../../web/cpp/game/Stage.hpp"
#include "../../web/cpp/game/StageTable.hpp"
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
    if ((command == "stage-run" || command == "stage-digest") && argc >= 5) {
        // stage-digest：同一状态流按 tools/oracle/stage-oracle.py 的规范字节序列做 FNV-1a 64，供 golden 对照
        const bool digest_only = command == "stage-digest";
        // StageManager 单独驱动（0x409db0 → 0x40add0 → 每帧 0x409e50），逐帧导出相机/脚本/雾/面 VM/层精灵 VM，
        // 与 tools/oracle/stage-oracle.py（Unicorn 直跑原版）对照
        const int stage = std::atoi(argv[3]);
        const int frames = std::atoi(argv[4]);
        const StageEntry& entry = stage_entry(stage);
        std::vector<u8> std_bytes;
        if (!archive.read(entry.std_name, std_bytes)) return fail(archive.error.c_str());
        StageManager st;
        if (!st.load(std_bytes.data(), u32(std_bytes.size()), stage)) return fail(st.error.c_str());
        std::vector<u8> anm_bytes;
        if (!archive.read(st.std.anm_name.c_str(), anm_bytes)) return fail(archive.error.c_str());
        AnmResource anm;
        if (!anm.parse(anm_bytes.data(), u32(anm_bytes.size()), st.std.anm_name.c_str())) return fail(anm.error.c_str());
        // 原版启动时 text.anm 已在槽 0：全零层精灵 VM（file_slot 0 / script 0）每帧会推进它的脚本 0
        std::vector<u8> text_bytes;
        AnmResource text;
        if (!archive.read("text.anm", text_bytes) || !text.parse(text_bytes.data(), u32(text_bytes.size()), "text.anm")) return fail("cannot load text.anm");
        AnmManager manager;
        manager.load(0, text);
        st.anm = manager.load(3 + (stage & 1), anm);
        if (st.anm == nullptr) return fail("stage anm load failed");
        st.start(manager);
        constexpr size_t kSnapshotBytes = 0x568;
        auto hex_vm = [&](const AnmVm& vm) {
            const u8* raw = reinterpret_cast<const u8*>(&vm);
            for (size_t i = 0; i < kSnapshotBytes; ++i) std::printf("%02x", raw[i]);
        };
        auto f3 = [&](const Vec3& v) { std::printf("[%u,%u,%u]", bits(v.x), bits(v.y), bits(v.z)); };
        // 规范字节流（小端 u32）：frame, timer.current, timer.current_f, ins, rocking_mode, rocking_timer, stage_frame,
        // flags, bg, cam[19]（pos facing up facing_norm pos_offset facing_offset fov）, fog[7], layer_ids[8], radius_sq,
        // 然后面 VM × 0x568 字节、层精灵 VM 8 × 0x568 字节
        u64 digest = 0xcbf29ce484222325ull;
        auto feed = [&](const u8* p, size_t n) { for (size_t i = 0; i < n; ++i) { digest ^= p[i]; digest *= 0x100000001b3ull; } };
        auto feed_u32 = [&](u32 v) { u8 b[4] = {u8(v), u8(v >> 8), u8(v >> 16), u8(v >> 24)}; feed(b, 4); };
        if (!digest_only) std::printf("{\"stage\":%d,\"std\":\"%s\",\"anm\":\"%s\",\"frames\":[", stage, entry.std_name, st.std.anm_name.c_str());
        for (int f = 0; f < frames; ++f) {
            st.update(manager, 1.0f);
            const StageCamera& c = st.camera;
            const i32 ins_offset = (st.ins_index >= 0 && size_t(st.ins_index) < st.std.script.size()) ? i32(st.std.script[size_t(st.ins_index)].offset) : -1;
            const u32 cam[19] = {bits(c.pos.x), bits(c.pos.y), bits(c.pos.z), bits(c.facing.x), bits(c.facing.y), bits(c.facing.z),
                                 bits(c.up.x), bits(c.up.y), bits(c.up.z), bits(c.facing_norm.x), bits(c.facing_norm.y), bits(c.facing_norm.z),
                                 bits(c.pos_offset.x), bits(c.pos_offset.y), bits(c.pos_offset.z),
                                 bits(c.facing_offset.x), bits(c.facing_offset.y), bits(c.facing_offset.z), bits(c.fov)};
            const u32 fog[7] = {bits(c.fog.near_), bits(c.fog.far_), bits(c.fog.c[0]), bits(c.fog.c[1]), bits(c.fog.c[2]), bits(c.fog.c[3]), c.fog.color};
            const u32 head[9] = {u32(f + 1), u32(st.timer.current), bits(st.timer.current_f), u32(ins_offset), u32(st.rocking_mode),
                                 u32(st.rocking_timer.current), u32(st.frame), st.flags, st.bg_color};
            for (u32 v : head) feed_u32(v);
            for (u32 v : cam) feed_u32(v);
            for (u32 v : fog) feed_u32(v);
            for (int i = 0; i < StageManager::kLayerSprites; ++i) feed_u32(u32(st.layer_ids[i]));
            feed_u32(bits(st.distortion_radius_sq));
            for (const AnmVm& vm : st.face_vms) feed(reinterpret_cast<const u8*>(&vm), kSnapshotBytes);
            for (const AnmVm& vm : st.layer_vms) feed(reinterpret_cast<const u8*>(&vm), kSnapshotBytes);
            if (digest_only) continue;
            std::printf("%s{\"frame\":%d,\"timer\":[%d,%u],\"ins\":%d,\"rocking\":[%d,%d],\"stage_frame\":%d,\"flags\":%u,\"bg\":%u,",
                        f == 0 ? "" : ",", f + 1, st.timer.current, bits(st.timer.current_f), ins_offset, st.rocking_mode, st.rocking_timer.current,
                        st.frame, st.flags, st.bg_color);
            std::printf("\"cam\":{\"pos\":"); f3(c.pos);
            std::printf(",\"facing\":"); f3(c.facing);
            std::printf(",\"up\":"); f3(c.up);
            std::printf(",\"facing_norm\":"); f3(c.facing_norm);
            std::printf(",\"pos_offset\":"); f3(c.pos_offset);
            std::printf(",\"facing_offset\":"); f3(c.facing_offset);
            std::printf(",\"fov\":%u},", bits(c.fov));
            std::printf("\"fog\":[%u,%u,%u,%u,%u,%u,%u],", fog[0], fog[1], fog[2], fog[3], fog[4], fog[5], fog[6]);
            std::printf("\"layer_ids\":[");
            for (int i = 0; i < StageManager::kLayerSprites; ++i) std::printf("%s%d", i ? "," : "", st.layer_ids[i]);
            std::printf("],\"radius_sq\":%u,\"faces\":[", bits(st.distortion_radius_sq));
            for (size_t i = 0; i < st.face_vms.size(); ++i) { std::printf("%s\"", i ? "," : ""); hex_vm(st.face_vms[i]); std::printf("\""); }
            std::printf("],\"layers\":[");
            for (int i = 0; i < StageManager::kLayerSprites; ++i) { std::printf("%s\"", i ? "," : ""); hex_vm(st.layer_vms[i]); std::printf("\""); }
            std::printf("]}");
        }
        if (digest_only) std::printf("stage=%d frames=%d faces=%zu digest=%016llx\n", stage, frames, st.face_vms.size(), (unsigned long long)digest);
        else std::printf("]}\n");
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
    if ((command == "game-run" || command == "game-digest") && argc >= 8) {
        // 直接进入场景 7（跳过启动幕/标题），每帧注入 held 位并按 tools/oracle/game-oracle.py 的规范字段导出 Player 状态。
        // 参数：<th16.dat> <stage> <shot> <season> <frames> <input "f:held,..."> [enemies "f:count,..."] [dialogue "f:v,..."]
        // 输入脚本语义与 game-oracle --input 相同：第 f 帧（0 起）起 held 持续到下一事件。
        // enemies/dialogue 为其它管理器未移植时的 PlayerWorld 外部输入（oracle 快照 enemy_count / dialogue 字段）。
        const bool digest_only = command == "game-digest";
        const int stage = std::atoi(argv[3]);
        const int shot = std::atoi(argv[4]);
        const int season = std::atoi(argv[5]);
        const int frames = std::atoi(argv[6]);
        struct Event { int frame; i32 value; };
        auto parse_schedule = [&](const char* p, std::vector<Event>& out) -> bool {
            while (*p) {
                char* endp = nullptr;
                const long frame = std::strtol(p, &endp, 10);
                if (endp == p || *endp != ':') return false;
                p = endp + 1;
                const long v = std::strtol(p, &endp, 0);
                out.push_back({int(frame), i32(v)});
                p = endp;
                if (*p == ',') ++p;
            }
            return true;
        };
        std::vector<Event> input_script, enemy_script, dialogue_script;
        if (!parse_schedule(argv[7], input_script)) return fail("bad input script");
        if (argc >= 9 && !parse_schedule(argv[8], enemy_script)) return fail("bad enemies script");
        if (argc >= 10 && !parse_schedule(argv[9], dialogue_script)) return fail("bad dialogue script");

        GameSession session;
        if (!session.open(dat.data(), u32(dat.size()))) return fail(session.failure.c_str());
        session.run_draw_chains = false;   // oracle 不跑 0x4015a0
        session.globals.stage = stage;
        session.globals.stage_copy = stage;
        session.globals.shot = shot;
        session.globals.season = season;
        session.globals.set_mode(0);       // 0x449190：普通模式默认 replay_slot = -1
        if (!enemy_script.empty()) {
            session.sync_enemy_count = false;
            session.globals.replay_slot = 0;  // 旧 player.json 桩在 replay_slot=0 下录制
        }
        if (argc >= 11 && argv[10][0] != '\0') session.globals.difficulty = std::atoi(argv[10]);
        if (argc >= 12 && argv[11][0] != '\0') session.globals.replay_slot = std::atoi(argv[11]);
        if (!session.start_game_direct()) return fail(session.failure.c_str());

        constexpr size_t kSnapshotBytes = 0x568;
        u64 digest = 0xcbf29ce484222325ull;
        u64 enm_digest = 0xcbf29ce484222325ull;
        u64 bul_digest = 0xcbf29ce484222325ull;
        auto feed = [&](const u8* p, size_t n) { for (size_t i = 0; i < n; ++i) { digest ^= p[i]; digest *= 0x100000001b3ull; } };
        auto feed_u32 = [&](u32 v) { u8 b[4] = {u8(v), u8(v >> 8), u8(v >> 16), u8(v >> 24)}; feed(b, 4); };
        auto feed_enm = [&](const u8* p, size_t n) { for (size_t i = 0; i < n; ++i) { enm_digest ^= p[i]; enm_digest *= 0x100000001b3ull; } };
        auto feed_bul_u32 = [&](u32 v) { for (int i = 0; i < 4; ++i) { bul_digest ^= u8(v >> (8 * i)); bul_digest *= 0x100000001b3ull; } };
        auto feed_enm_u32 = [&](u32 v) { u8 b[4] = {u8(v), u8(v >> 8), u8(v >> 16), u8(v >> 24)}; feed_enm(b, 4); };
        auto timer3 = [&](const Timer& t, std::vector<u32>& w) { w.push_back(u32(t.previous)); w.push_back(u32(t.current)); w.push_back(bits(t.current_f)); };
        auto vec3 = [&](const Vec3& v, std::vector<u32>& w) { w.push_back(bits(v.x)); w.push_back(bits(v.y)); w.push_back(bits(v.z)); };
        auto has = [](u32 id) -> u32 { return id != 0 ? 1u : 0u; };
        auto print_words = [&](const char* key, const std::vector<u32>& w, size_t from, size_t count) {
            std::printf("\"%s\":[", key);
            for (size_t i = 0; i < count; ++i) std::printf("%s%u", i ? "," : "", w[from + i]);
            std::printf("]");
        };

        if (!digest_only) std::printf("{\"stage\":%d,\"shot\":%d,\"season\":%d,\"frames\":[", stage, shot, season);
        i32 held = 0;
        i32 enemy_hold = -1;
        i32 dialogue_hold = -1;
        // 提供了 enemies/dialogue 时间表时，由桩接管 PlayerWorld 对应字段（保持“最后一次赋值”语义）
        if (!enemy_script.empty()) session.sync_enemy_count = false;
        for (int f = 0; f < frames; ++f) {
            for (const Event& e : input_script) if (e.frame == f) held = e.value;
            for (const Event& e : enemy_script) if (e.frame == f) enemy_hold = e.value;
            for (const Event& e : dialogue_script) if (e.frame == f) dialogue_hold = e.value;
            if (enemy_hold >= 0) session.game.world.enemy_count = enemy_hold;
            if (dialogue_hold >= 0) session.game.world.dialogue = dialogue_hold != 0;
            session.injected_input = u32(held);
            if (!session.logic_step()) return fail(session.failure.c_str());

            const Player& pl = session.game.player;
            const GameGlobals& g = session.globals;
            std::vector<u32> w;
            w.push_back(u32(f + 1));
            w.push_back(u32(g.game_frame));
            w.push_back(u32(g.stage_frame));
            w.push_back(session.game_input.held);
            w.push_back(session.game_input.pressed);
            w.push_back(u32(g.power));
            w.push_back(u32(g.season_gauge));
            const size_t player_begin = w.size();
            w.push_back(u32(pl.state));
            vec3(pl.pos, w);
            w.push_back(u32(pl.fixed_x)); w.push_back(u32(pl.fixed_y));
            timer3(pl.state_timer, w); timer3(pl.timer2, w); timer3(pl.timer3, w);
            w.push_back(u32(pl.direction));
            w.push_back(u32(pl.last_move_x)); w.push_back(u32(pl.last_move_y));
            w.push_back(u32(pl.focus)); w.push_back(has(pl.focus_vm));
            w.push_back(u32(pl.speed)); w.push_back(u32(pl.speed_focus)); w.push_back(u32(pl.speed_diag)); w.push_back(u32(pl.speed_diag_focus));
            vec3(pl.velocity, w); vec3(pl.last_velocity, w);
            w.push_back(u32(pl.velocity_ix)); w.push_back(u32(pl.velocity_iy));
            w.push_back(u32(pl.option_lag)); w.push_back(u32(pl.gather_counter)); w.push_back(bits(pl.speed_mult));
            vec3(pl.push, w);
            w.push_back(u32(pl.option_count)); w.push_back(u32(pl.power_level)); w.push_back(u32(pl.sub_count)); w.push_back(pl.flags);
            timer3(pl.invincible, w);
            timer3(pl.shot_timer, w); timer3(pl.shot_timer2, w);
            vec3(pl.rect_min, w); vec3(pl.rect_max, w);
            vec3(pl.hit_radius, w); vec3(pl.item_radius, w); vec3(pl.graze_radius, w);
            vec3(pl.item_rect_min, w); vec3(pl.item_rect_max, w);
            vec3(pl.graze_rect_min, w); vec3(pl.graze_rect_max, w);
            vec3(pl.item_rect2_min, w); vec3(pl.item_rect2_max, w);
            w.push_back(u32(pl.scale_active)); w.push_back(bits(pl.scale));
            const size_t options_begin = w.size();
            auto push_option = [&](const PlayerOption& o) {
                w.push_back(u32(o.active));
                w.push_back(u32(o.target_x)); w.push_back(u32(o.target_y));
                w.push_back(u32(o.x)); w.push_back(u32(o.y));
                w.push_back(u32(o.offset_x)); w.push_back(u32(o.offset_y)); w.push_back(u32(o.focus_offset_x)); w.push_back(u32(o.focus_offset_y));
                w.push_back(has(o.vm)); w.push_back(has(o.vm_front));
                w.push_back(u32(o.index)); w.push_back(u32(o.snap));
            };
            for (const PlayerOption& o : pl.options) push_option(o);
            for (const PlayerOption& o : pl.sub_options) push_option(o);
            // 射击系统：shot_misc、类型 2 冷却表、上次伤害源下标、活动自机弹、活动伤害源、本帧音效
            const size_t shooting_begin = w.size();
            w.push_back(u32(pl.shot_misc)); w.push_back(u32(pl.shot_misc_b));
            for (i32 c : pl.option_cooldown) w.push_back(u32(c));
            w.push_back(u32(pl.last_damage_index));
            std::vector<const PlayerBullet*> live_bullets;
            for (const PlayerBullet& b : pl.bullets) if (b.active != 0) live_bullets.push_back(&b);
            w.push_back(u32(live_bullets.size()));
            for (const PlayerBullet* b : live_bullets) {
                w.push_back(u32(b->index)); w.push_back(b->flags); w.push_back(has(b->vm));
                timer3(b->timer, w);
                vec3(b->motion.pos, w); w.push_back(bits(b->motion.speed)); w.push_back(bits(b->motion.angle));
                vec3(b->motion.velocity, w);
                w.push_back(b->motion.mode); w.push_back(u32(b->active)); w.push_back(u32(b->target)); w.push_back(u32(b->damage));
                w.push_back(bits(b->hit_w)); w.push_back(bits(b->hit_h));
                w.push_back(b->shot_id); w.push_back(u32(b->damage_source));
            }
            std::vector<int> live_sources;
            for (int i = 0; i < Player::kDamageSourceCount; ++i) if ((pl.damage_sources[i].flags & 1u) != 0) live_sources.push_back(i);
            w.push_back(u32(live_sources.size()));
            for (int i : live_sources) {
                const DamageSource& d = pl.damage_sources[i];
                w.push_back(u32(i + 1)); w.push_back(d.flags); w.push_back(bits(d.f04)); w.push_back(bits(d.f08));
                w.push_back(bits(d.angle)); w.push_back(bits(d.angular_speed)); w.push_back(bits(d.size_x)); w.push_back(bits(d.size_y));
                vec3(d.motion.pos, w);
                const u32* motion_rest = reinterpret_cast<const u32*>(&d.motion) + 3;   // +0x28..+0x5f：pos2 … mode
                for (int k = 0; k < 14; ++k) w.push_back(motion_rest[k]);
                timer3(d.timer, w);
                w.push_back(u32(d.damage));
                w.push_back(u32(d.f78)); w.push_back(u32(d.f7c)); w.push_back(u32(d.f80)); w.push_back(u32(d.f84));
                w.push_back(u32(d.owner)); w.push_back(u32(d.f8c)); w.push_back(u32(d.kind));
            }
            // oracle 只记录 Player 代码段（0x440000..0x449000）登记的音效
            w.push_back(u32(session.player_sfx_events.size()));
            for (i32 s : session.player_sfx_events) w.push_back(u32(s));
            for (u32 v : w) feed_u32(v);
            feed(reinterpret_cast<const u8*>(&pl.vm), kSnapshotBytes);
            {
                u32 live_enm = 0;
                for (const auto& ep : session.game.enemies.enemies) if (ep) ++live_enm;
                feed_enm_u32(u32(f + 1));
                feed_enm_u32(u32(session.game.enemies.enemy_count));
                feed_enm_u32(live_enm);
                for (const auto& ep : session.game.enemies.enemies) {
                    if (!ep) continue;
                    const Enemy& e = *ep;
                    feed_enm_u32(e.id);
                    feed_enm_u32(e.flags);
                    const u32* m1 = reinterpret_cast<const u32*>(&e.motion1);
                    const u32* m2 = reinterpret_cast<const u32*>(&e.motion2);
                    for (int k = 0; k < 17; ++k) feed_enm_u32(m1[k]);
                    for (int k = 0; k < 17; ++k) feed_enm_u32(m2[k]);
                    feed_enm_u32(bits(e.primary.time));
                    feed_enm_u32(u32(e.primary.routine));
                    feed_enm_u32(u32(e.primary.instruction_offset));
                    feed_enm_u32(u32(e.life));
                    feed_enm_u32(u32(e.life_max));
                    if (const char* rn = e.routine_name()) feed_enm(reinterpret_cast<const u8*>(rn), std::strlen(rn));
                }
            }
            // 敌弹（game-oracle.py bullet_digest 同口径）：[frame, count, len] + 每弹 25 字
            {
                u32 live_bul = 0;
                const BulletManager* bm = session.game.bullets_live ? session.game.bullets.get() : nullptr;
                if (bm) for (const Bullet* b = bm->active_head; b; b = b->active_next) ++live_bul;
                feed_bul_u32(u32(f + 1));
                feed_bul_u32(u32(bm ? bm->active_count : 0));
                feed_bul_u32(live_bul);
                if (bm) for (const Bullet* b = bm->active_head; b; b = b->active_next) {
                    const u32 words[] = {u32(b->index), b->flags, u32(b->state), u32(i32(b->type)), u32(i32(b->color)),
                                         bits(b->pos.x), bits(b->pos.y), bits(b->pos.z),
                                         bits(b->velocity.x), bits(b->velocity.y), bits(b->velocity.z),
                                         bits(b->speed), bits(b->angle), bits(b->hitbox.x), bits(b->hitbox.y),
                                         bits(b->scale), u32(b->ex_index), b->ex_flags,
                                         u32(b->timer.previous), u32(b->timer.current), bits(b->timer.current_f),
                                         u32(b->vm_main.sprite_index)};
                    for (u32 v : words) feed_bul_u32(v);
                }
            }
            if (digest_only) continue;

            std::printf("%s{\"frame\":%d,\"game_frame\":%d,\"stage_frame\":%d,\"input\":[%u,%u],\"power\":%d,\"season_gauge\":%d,"
                        "\"enemy_count\":%d,\"dialogue\":%d,\"rng\":[%u,%u],\"player\":{",
                        f == 0 ? "" : ",", f + 1, g.game_frame, g.stage_frame, session.game_input.held, session.game_input.pressed,
                        g.power, g.season_gauge, session.game.world.enemy_count, session.game.world.dialogue ? 1 : 0,
                        u32(session.animations.visual_rng.seed), session.animations.visual_rng.calls);
            size_t at = player_begin;
            std::printf("\"state\":%d,", i32(w[at])); at += 1;
            print_words("pos", w, at, 3); at += 3; std::printf(",");
            std::printf("\"pos_fixed\":[%d,%d],", i32(w[at]), i32(w[at + 1])); at += 2;
            std::printf("\"timers\":[");
            for (int t = 0; t < 3; ++t) { std::printf("%s[%d,%d,%u]", t ? "," : "", i32(w[at]), i32(w[at + 1]), w[at + 2]); at += 3; }
            std::printf("],");
            std::printf("\"dir\":%d,\"last_move\":[%d,%d],\"focus\":%d,\"focus_vm\":%u,", i32(w[at]), i32(w[at + 1]), i32(w[at + 2]), i32(w[at + 3]), w[at + 4]); at += 5;
            print_words("speeds", w, at, 4); at += 4; std::printf(",");
            print_words("velocity", w, at, 8); at += 8; std::printf(",");
            std::printf("\"option_lag\":%d,\"counter\":%d,\"speed_mult\":%u,", i32(w[at]), i32(w[at + 1]), w[at + 2]); at += 3;
            print_words("ext_velocity", w, at, 3); at += 3; std::printf(",");
            std::printf("\"option_count\":%d,\"power_level\":%d,\"sub_count\":%d,\"flags\":%u,", i32(w[at]), i32(w[at + 1]), i32(w[at + 2]), w[at + 3]); at += 4;
            print_words("invincible", w, at, 3); at += 3; std::printf(",");
            std::printf("\"shot_timers\":[");
            for (int t = 0; t < 2; ++t) { std::printf("%s[%d,%d,%u]", t ? "," : "", i32(w[at]), i32(w[at + 1]), w[at + 2]); at += 3; }
            std::printf("],");
            print_words("rects", w, at, 6); at += 6; std::printf(",");
            print_words("radii", w, at, 9); at += 9; std::printf(",");
            print_words("rects2", w, at, 18); at += 18; std::printf(",");
            std::printf("\"scale\":[%d,%u],", i32(w[at]), w[at + 1]); at += 2;
            if (at != options_begin) return fail("snapshot layout mismatch");
            std::printf("\"options\":[");
            for (int i = 0; i < Player::kOptionCount + Player::kSubOptionCount; ++i) {
                const u32* o = &w[options_begin + size_t(i) * 13];
                std::printf("%s{\"active\":%d,\"target\":[%d,%d],\"pos\":[%d,%d],\"offset\":[%d,%d,%d,%d],\"vm\":[%u,%u],\"index\":%d,\"snap\":%d}",
                            i ? "," : "", i32(o[0]), i32(o[1]), i32(o[2]), i32(o[3]), i32(o[4]), i32(o[5]), i32(o[6]), i32(o[7]), i32(o[8]),
                            o[9], o[10], i32(o[11]), i32(o[12]));
            }
            std::printf("],");
            at = shooting_begin;
            std::printf("\"shot_misc\":[%d,%u],", i32(w[at]), w[at + 1]); at += 2;
            print_words("cooldown", w, at, 16); at += 16; std::printf(",");
            std::printf("\"last_damage_index\":%d,", i32(w[at])); at += 1;
            const u32 bullet_count = w[at]; at += 1;
            std::printf("\"bullets\":[");
            for (u32 i = 0; i < bullet_count; ++i) {
                const u32* b = &w[at];
                std::printf("%s{\"index\":%d,\"flags\":%u,\"vm\":%u,\"timer\":[%d,%d,%u],\"pos\":[%u,%u,%u],\"speed\":%u,\"angle\":%u,"
                            "\"velocity\":[%u,%u,%u],\"mode\":%u,\"active\":%d,\"target\":%d,\"damage\":%d,\"size\":[%u,%u],\"shot_id\":%u,\"damage_source\":%d}",
                            i ? "," : "", i32(b[0]), b[1], b[2], i32(b[3]), i32(b[4]), b[5], b[6], b[7], b[8], b[9], b[10],
                            b[11], b[12], b[13], b[14], i32(b[15]), i32(b[16]), i32(b[17]), b[18], b[19], b[20], i32(b[21]));
                at += 22;
            }
            std::printf("],");
            const u32 source_count = w[at]; at += 1;
            std::printf("\"damage_sources\":[");
            for (u32 i = 0; i < source_count; ++i) {
                const u32* d = &w[at];
                std::printf("%s{\"index\":%d,\"flags\":%u,\"f04\":[%u,%u],\"angle\":%u,\"angular_speed\":%u,\"size\":[%u,%u],\"pos\":[%u,%u,%u],",
                            i ? "," : "", i32(d[0]), d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10]);
                print_words("motion", w, at + 11, 14);
                std::printf(",\"timer\":[%d,%d,%u],\"damage\":%d,", i32(d[25]), i32(d[26]), d[27], i32(d[28]));
                print_words("tail", w, at + 29, 7);
                std::printf("}");
                at += 36;
            }
            std::printf("],");
            const u32 sfx_count = w[at]; at += 1;
            print_words("sfx", w, at, sfx_count); at += sfx_count;
            if (at != w.size()) return fail("snapshot layout mismatch (shooting)");
            std::printf(",\"vm\":\"");
            const u8* raw = reinterpret_cast<const u8*>(&pl.vm);
            for (size_t i = 0; i < kSnapshotBytes; ++i) std::printf("%02x", raw[i]);
            std::printf("\"},\"enemies\":{\"count\":%d,\"enemies\":[", session.game.enemies.enemy_count);
            bool first_enm = true;
            for (const auto& ep : session.game.enemies.enemies) {
                if (!ep) continue;
                const Enemy& e = *ep;
                const char* rname = e.routine_name();
                std::printf("%s{\"id\":%u,\"flags\":%u,\"pos\":[%.9g,%.9g,%.9g],",
                            first_enm ? "" : ",", e.id, e.flags, e.pos.x, e.pos.y, e.pos.z);
                first_enm = false;
                auto print_motion = [&](const char* key, const MotionState& m) {
                    const u32* mw = reinterpret_cast<const u32*>(&m);
                    std::printf("\"%s\":[", key);
                    for (int k = 0; k < 17; ++k) std::printf("%s%u", k ? "," : "", mw[k]);
                    std::printf("]");
                };
                print_motion("motion1", e.motion1); std::printf(",");
                print_motion("motion2", e.motion2); std::printf(",");
                std::printf("\"ctx\":[%.9g,%d,%d,", e.primary.time, e.primary.routine, e.primary.instruction_offset);
                if (rname) std::printf("\"%s\"", rname); else std::printf("null");
                std::printf("],\"hp\":[%d,%d,0,0]}", e.life, e.life_max);
            }
            std::printf("]}");
            {
                const BulletManager* bm = session.game.bullets_live ? session.game.bullets.get() : nullptr;
                std::printf(",\"bullets\":{\"count\":%d,\"bullets\":[", bm ? bm->active_count : 0);
                bool first_bul = true;
                if (bm) for (const Bullet* b = bm->active_head; b; b = b->active_next) {
                    std::printf("%s{\"index\":%d,\"flags\":%u,\"state\":%u,\"type\":%d,\"color\":%d,\"pos\":[%u,%u,%u],\"vel\":[%u,%u,%u],"
                                "\"speed\":%u,\"angle\":%u,\"hitbox\":[%u,%u],\"scale\":%u,\"ex_index\":%d,\"ex_flags\":%u,"
                                "\"timer\":[%d,%d,%u],\"sprite_id\":%d}",
                                first_bul ? "" : ",", b->index, b->flags, u32(b->state), i32(b->type), i32(b->color),
                                bits(b->pos.x), bits(b->pos.y), bits(b->pos.z), bits(b->velocity.x), bits(b->velocity.y), bits(b->velocity.z),
                                bits(b->speed), bits(b->angle), bits(b->hitbox.x), bits(b->hitbox.y), bits(b->scale), b->ex_index, b->ex_flags,
                                b->timer.previous, b->timer.current, bits(b->timer.current_f), b->vm_main.sprite_index);
                    first_bul = false;
                }
                std::printf("]}");
            }
            std::printf("}");
        }
        if (digest_only)
            std::printf("stage=%d shot=%d season=%d frames=%d digest=%016llx enemies=%016llx bullets=%016llx\n",
                        stage, shot, season, frames, (unsigned long long)digest, (unsigned long long)enm_digest,
                        (unsigned long long)bul_digest);
        else std::printf("]}\n");
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
