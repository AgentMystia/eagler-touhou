// WASI 无头 harness：同一份 game 源码在 wasm32-wasip1 下独立运行，
// 供 CI 中的 oracle 对照（原版 Unicorn 轨迹 vs C++ 行为）调用。
// 编译方式见 portable/build-harness.mjs（WASI_SDK_PATH 需指向 wasi-sdk）。
// 用法：
//   th13-harness              旧有 RNG 冒烟（8 值输出）
//   th13-harness stage1 <资源目录> <事件输出路径>
//     资源目录需含 st01.ecl 及其 ECLI/ANIM 依赖（artifacts/reference/raw）。
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "game/Rng.hpp"
#include "game/StageSim.hpp"

namespace {

int rng_smoke() {
    th13::Rng rng(1);  // MSVC CRT 默认 srand(1)
    std::printf("th13-harness rng");
    for (int i = 0; i < 8; i++) {
        std::printf(" %d", rng.next());
    }
    std::printf("\n");
    return 0;
}

int stage1(const char* resource_dir, const char* event_path, unsigned frames) {
    th13::StageSim sim;
    if (!sim.load(resource_dir, event_path)) {
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

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 4 && std::strcmp(argv[1], "stage1") == 0)
        return stage1(argv[2], argv[3], argc >= 5 ? unsigned(std::atoi(argv[4])) : 3600u);
    return rng_smoke();
}
