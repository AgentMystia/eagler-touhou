// th16 浏览器宿主：Emscripten 导出集中一处（页面壳 th16.html 调用）。
//   th16_initialize()      打开 /assets/th16.dat、建 SDL3 窗口 + GLES 3.0 上下文、装载启动幕资源；存档根 /save
//   th16_loop_start()      requestAnimationFrame 主循环
//   th16_frame_tick()      手动泵一帧（无 rAF 环境 / 截图自证）
//   th16_frame()/th16_error()/th16_phase() 观测面
#include "Application.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <memory>
#include <string>

namespace {
std::unique_ptr<th16::sdl::Application> app;
std::string last_error;
bool running = false;
uintptr_t loop_epoch = 0;
}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE int th16_initialize() {
    if (app && app->initialized) return 1;
    app = std::make_unique<th16::sdl::Application>();
    // scoreth16.dat 与 replay/th16_XX.rpy 落在页面壳挂的 IDBFS /save，写盘后由壳回写 IndexedDB
    app->session.save_directory = "/save";
    app->session.on_saved = [] { EM_ASM({ if (Module.th16_persist) Module.th16_persist(); }); };
    if (!app->initialize("/assets/th16.dat")) {
        last_error = app->failure;
        app.reset();
        return 0;
    }
    return 1;
}

EMSCRIPTEN_KEEPALIVE const char* th16_error() { return last_error.c_str(); }
EMSCRIPTEN_KEEPALIVE unsigned th16_frame() { return app ? app->frames : 0; }
// 0 = 未初始化，1 = 启动幕，2 = 循环运行中
EMSCRIPTEN_KEEPALIVE unsigned th16_phase() { return !app || !app->initialized ? 0 : running ? 2 : 1; }
// DAT_004c17c4 口径：1 = Boot，4 = Title；未初始化返回 0
EMSCRIPTEN_KEEPALIVE int th16_scene() {
    if (!app || !app->initialized) return 0;
    return int(app->supervisor().current);
}
EMSCRIPTEN_KEEPALIVE unsigned th16_boot_frames() {
    return app ? app->supervisor().boot_frames : 0;
}
EMSCRIPTEN_KEEPALIVE int th16_title_live() {
    return app && app->supervisor().title_menu_live ? 1 : 0;
}
// 标题观测：画面（+0x18）/ 子状态（+0x20）/ 光标（+0x24）/ 全局难度·机体·季节·关卡 / 退出请求
EMSCRIPTEN_KEEPALIVE int th16_title_screen() { return app ? app->title().screen : -1; }
EMSCRIPTEN_KEEPALIVE int th16_title_sub() { return app ? app->title().sub : -1; }
EMSCRIPTEN_KEEPALIVE int th16_title_cursor() { return app ? app->title().cursor.index : -1; }
EMSCRIPTEN_KEEPALIVE int th16_difficulty() { return app ? app->session.globals.difficulty : -1; }
EMSCRIPTEN_KEEPALIVE int th16_shot() { return app ? app->session.globals.shot : -1; }
EMSCRIPTEN_KEEPALIVE int th16_season() { return app ? app->session.globals.season : -1; }
EMSCRIPTEN_KEEPALIVE int th16_stage() { return app ? app->session.globals.stage : -1; }
EMSCRIPTEN_KEEPALIVE int th16_quit_requested() { return app && app->session.quit_requested ? 1 : 0; }
// 测试注入：与键盘采样 OR 的 held 位（InputState 位布局：bit0 Z, bit1 X, bit4 ↑, bit5 ↓, bit8 Esc, bit19 Enter）
EMSCRIPTEN_KEEPALIVE void th16_inject_input(unsigned bits) { if (app) app->session.injected_input = bits; }
// 最近逻辑步数（FrameClock 追帧观测）
EMSCRIPTEN_KEEPALIVE unsigned th16_last_subframes() { return app ? app->clock.subframes : 0; }

EMSCRIPTEN_KEEPALIVE int th16_frame_tick() {
    if (!app || !app->initialized) return 0;
    if (!app->step()) { last_error = app->failure; return 0; }
    return 1;
}

EMSCRIPTEN_KEEPALIVE void th16_loop_stop() { running = false; ++loop_epoch; }

EMSCRIPTEN_KEEPALIVE void th16_loop_start() {
    if (running || !app || !app->initialized) return;
    running = true;
    emscripten_request_animation_frame_loop(
        [](double time_ms, void* epoch) -> EM_BOOL {
            if (!running || uintptr_t(epoch) != loop_epoch) return EM_FALSE;
            if (!app->step_at(time_ms / 1000.0)) { last_error = app->failure; running = false; return EM_FALSE; }
            return EM_TRUE;
        },
        reinterpret_cast<void*>(uintptr_t(++loop_epoch)));
}

}  // extern "C"
