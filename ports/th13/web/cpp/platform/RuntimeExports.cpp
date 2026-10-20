// 浏览器运行时导出：启动入口与 RNG 对照接口。
// WASI harness 目标下编译为空实现（TH13_WASI_HARNESS），保持双目标同源。
#include "game/Rng.hpp"

#ifndef TH13_WASI_HARNESS
#include <emscripten/emscripten.h>
#include "sdl/ApplicationHost.hpp"

extern "C" {

EMSCRIPTEN_KEEPALIVE
int th13Boot() {
    static th13::sdl::ApplicationHost host;
    return host.run();
}

EMSCRIPTEN_KEEPALIVE
void th13RngSeed(th13::Rng* rng, uint32_t seed) { rng->reseed(seed); }

EMSCRIPTEN_KEEPALIVE
uint32_t th13RngNext(th13::Rng* rng) { return rng->next(); }

EMSCRIPTEN_KEEPALIVE
uint32_t th13RngState(const th13::Rng* rng) { return rng->state(); }

}  // extern "C"
#endif
