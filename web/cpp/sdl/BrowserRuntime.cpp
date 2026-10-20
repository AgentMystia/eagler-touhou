// 浏览器宿主（th15 BrowserRuntime 裁剪：EM 导出 + RAF 循环 + FrameCadence；
// 触控/thprac/practice lab 全部裁掉，docs/plans/ui-base.md §三）。
// 输入：SDL3 emscripten 端口把键盘事件汇入 SDL 键位态，再经
// GameKeyboard::keyboard_keys 生成 32 位游戏掩码（KeyboardMap.inc 同代布局）。
#include "ApplicationState.hpp"
#include "../game/GameInput.hpp"
#include "../game/GameKeyboard.hpp"
#include "../../../portable/sdl/FrameCadence.hpp"

#include <SDL3/SDL.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <cmath>
#include <memory>

EM_JS(void, th13_browser_frame, (int ok, double milliseconds, unsigned ticks), {
    Module["onGameFrame"]?.(ok, milliseconds, ticks);
});

namespace th13::sdl {
namespace {
std::unique_ptr<ApplicationState> app;
std::string last_error;
struct Key {
    const char* code;
    const char* sdl;
    u32 scan, vk;
    bool hosted = false;
    SDL_Scancode native = SDL_SCANCODE_UNKNOWN;
};
#include "../../../portable/input/KeyboardMap.inc"

GameInput controls;
touhou::sdl::FrameCadence cadence;
bool running = false, suspended = false, lost_focus = false;
u32 render_scale = 1, loop_epoch = 0;
double previous_frame = -1;

void clear_inputs() {
    for (auto& key : keyboard_map) key.hosted = false;
    SDL_ResetKeyboard();
    controls = {};
}

bool sample_and_tick() {
    if (!app) return false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            clear_inputs();
            lost_focus = true;
        } else if (event.type == SDL_EVENT_QUIT) {
            app->exiting = true;
        }
    }
    bool keys[256]{};
    const auto* physical = SDL_GetKeyboardState(nullptr);
    for (const auto& key : keyboard_map) {
        if (key.hosted || (!key.hosted && key.native != SDL_SCANCODE_UNKNOWN && physical[key.native])) {
            if (key.vk < 256) keys[key.vk] = true;
            if (key.vk >= 160 && key.vk <= 165) keys[16 + (key.vk - 160) / 2] = true;  // 双 Shift 归一
            if (key.scan == 28 || key.scan == 156) keys[13] = true;                    // Enter
        }
    }
    const u32 held = keyboard_keys(keys);
    controls.update(held);
    const bool focus = lost_focus;
    lost_focus = false;
    return app->step(controls.held, controls.pressed, controls.repeated, 60.f, focus);
}
}  // namespace
}  // namespace th13::sdl

extern "C" {

EMSCRIPTEN_KEEPALIVE int th13_prepare_loading() {
    using namespace th13::sdl;
    if (running) return 0;
    app = std::make_unique<ApplicationState>();
    app->graphics.render_scale = render_scale;
    return app->prepare_loading();
}

EMSCRIPTEN_KEEPALIVE int th13_draw_startup(unsigned frames) {
    using namespace th13::sdl;
    // 共享外壳的启动品牌动画期间推进装载 ANM（独立时钟），最多 120 帧。
    if (!app || running) return 0;
    for (unsigned frame = 0; frame < std::min(frames, 120u); ++frame) {
        if (!app->loading_animations.update(false) || !app->loading_animations.update(true))
            return 0;
        ++app->loading_frames;
    }
    return app->render_loading();
}

EMSCRIPTEN_KEEPALIVE int th13_initialize() {
    using namespace th13::sdl;
    if (running) return 1;
    clear_inputs();
    if (!app || !app->platform_prepared || app->initialized) {
        app = std::make_unique<ApplicationState>();
        app->graphics.render_scale = render_scale;
    }
    if (!app->initialize(true)) {
        last_error = app->failure;
        app.reset();
        return 0;
    }
    app->graphics.release_startup_branding();
    for (auto& key : keyboard_map) key.native = SDL_GetScancodeFromName(key.sdl);
    previous_frame = -1;
    cadence.reset();
    return 1;
}

EMSCRIPTEN_KEEPALIVE void th13_render_scale(unsigned scale) {
    if (!th13::sdl::running) th13::sdl::render_scale = scale >= 2 ? 2 : 1;
}

EMSCRIPTEN_KEEPALIVE const char* th13_error() {
    using namespace th13::sdl;
    return app ? app->failure.c_str() : last_error.c_str();
}

EMSCRIPTEN_KEEPALIVE unsigned th13_phase() {
    using namespace th13::sdl;
    return !app ? 4 : app->exiting ? 4 : app->title ? 0 : app->pause && app->pause->active() ? 2 : 1;
}

EMSCRIPTEN_KEEPALIVE unsigned th13_frame() { return th13::sdl::app ? th13::sdl::app->frames : 0; }

EMSCRIPTEN_KEEPALIVE const int* th13_probe_state() {
    using namespace th13::sdl;
    return app ? app->projection() : nullptr;
}

EMSCRIPTEN_KEEPALIVE void th13_key(unsigned scan, unsigned down) {
    for (auto& key : th13::sdl::keyboard_map)
        if (key.scan == scan) key.hosted = down != 0;
}

EMSCRIPTEN_KEEPALIVE void th13_keys_clear() { th13::sdl::clear_inputs(); }

EMSCRIPTEN_KEEPALIVE void th13_loop_stop() {
    using namespace th13::sdl;
    running = false;
    ++loop_epoch;
    previous_frame = -1;
    cadence.reset();
    clear_inputs();
    if (app) app->audio_device.suspend(true);
}

EMSCRIPTEN_KEEPALIVE void th13_loop_pause(unsigned enabled) {
    using namespace th13::sdl;
    suspended = enabled != 0;
    previous_frame = -1;
    cadence.reset();
    if (app) app->audio_device.suspend(suspended);
}

EMSCRIPTEN_KEEPALIVE void th13_loop_start() {
    using namespace th13::sdl;
    if (running || !app || !app->initialized) return;
    // 启动保留原生 180 帧装载动画（含外壳品牌期已推进部分）；场景切换只等资源。
    if (app->loading_frames >= 120) {
        app->loading_startup = false;
        app->loading_debt = 0;
    }
    running = true;
    suspended = false;
    previous_frame = -1;
    cadence.reset();
    clear_inputs();
    app->audio_device.suspend(false);
    emscripten_request_animation_frame_loop(
        [](double time, void* epoch) -> EM_BOOL {
            if (!running || uintptr_t(epoch) != loop_epoch) return EM_FALSE;
            const double begin = emscripten_get_now();
            const double delta = previous_frame < 0 ? 0 : (time - previous_frame) / 1000.;
            previous_frame = time;
            if (suspended) {
                cadence.reset();
                return EM_TRUE;
            }
            if (app->loading_waiting()) {
                cadence.reset();
                if (!(app->loading_startup ? app->advance_loading(delta) : app->complete_loading())) {
                    running = false;
                    th13_browser_frame(0, emscripten_get_now() - begin, 0);
                    return EM_FALSE;
                }
                app->audio_device.pump();
                return EM_TRUE;
            }
            const auto ticks = cadence.advance(delta);
            bool ok = true;
            app->graphics.backend.defer = true;
            for (unsigned i = 0; i < ticks && ok; i++) {
                ok = sample_and_tick();
                if (app->exiting || app->pending_load) break;
            }
            app->graphics.backend.commit();
            app->graphics.backend.defer = false;
            app->audio_device.pump();
            th13_browser_frame(ok ? 1 : 0, emscripten_get_now() - begin, ticks);
            if (!ok) running = false;
            return running ? EM_TRUE : EM_FALSE;
        },
        reinterpret_cast<void*>(uintptr_t(++loop_epoch)));
}
}  // extern "C"
