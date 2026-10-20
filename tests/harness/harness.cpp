// WASI 无头 harness：同一份 game 源码在 wasm32-wasip1 下独立运行，
// 供 CI 中的 oracle 对照（原版 Unicorn 轨迹 vs C++ 行为）调用。
// 编译方式见 portable/build-harness.mjs（WASI_SDK_PATH 需指向 wasi-sdk）。
#include <cstdio>

#include "game/Rng.hpp"

int main() {
    th13::Rng rng(0);
    std::printf("th13-harness rng");
    for (int i = 0; i < 8; i++) {
        std::printf(" %08x", rng.next());
    }
    std::printf("\n");
    return 0;
}
