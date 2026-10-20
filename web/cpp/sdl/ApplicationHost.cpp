#include "sdl/ApplicationHost.hpp"

#include <SDL3/SDL.h>

#include "game/Rng.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace th13::sdl {

namespace {
Rng g_frameRng;
ApplicationHost* g_host = nullptr;

void tickThunk() { g_host->tick(); }
}  // namespace

void ApplicationHost::tick() {
    frameCount_++;
    g_frameRng.next();  // 帧驱动 RNG 推进，供 oracle 逐帧比对
}

int ApplicationHost::run() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    g_host = this;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(tickThunk, 0, 1);
#else
    while (frameCount_ < 1'000'000) tick();
#endif
    SDL_Quit();
    return 0;
}

}  // namespace th13::sdl
