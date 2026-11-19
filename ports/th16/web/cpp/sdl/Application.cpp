#include "Application.hpp"

#include <cstdio>
#include <cstring>

#if defined(__EMSCRIPTEN__) || defined(TH16_HAS_SDL)
#include <SDL3/SDL.h>
#endif

namespace th16::sdl {

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

}  // namespace

// 0x401d50 键盘位（含 Enter/Esc/Ctrl 等菜单键）
void Application::sample_input() {
    u32 held = 0;
#if defined(__EMSCRIPTEN__) || defined(TH16_HAS_SDL)
    int n = 0;
    const bool* keys = SDL_GetKeyboardState(&n);
    if (keys != nullptr) {
        auto down = [&](SDL_Scancode s) { return s < n && keys[s]; };
        if (down(SDL_SCANCODE_Z)) held |= InputState::kShot;
        if (down(SDL_SCANCODE_X)) held |= InputState::kBomb;
        if (down(SDL_SCANCODE_LSHIFT) || down(SDL_SCANCODE_RSHIFT)) held |= InputState::kSlow;
        if (down(SDL_SCANCODE_UP) || down(SDL_SCANCODE_KP_8)) held |= InputState::kUp;
        if (down(SDL_SCANCODE_DOWN) || down(SDL_SCANCODE_KP_2)) held |= InputState::kDown;
        if (down(SDL_SCANCODE_LEFT) || down(SDL_SCANCODE_KP_4)) held |= InputState::kLeft;
        if (down(SDL_SCANCODE_RIGHT) || down(SDL_SCANCODE_KP_6)) held |= InputState::kRight;
        if (down(SDL_SCANCODE_KP_7)) held |= InputState::kUp | InputState::kLeft;
        if (down(SDL_SCANCODE_KP_9)) held |= InputState::kUp | InputState::kRight;
        if (down(SDL_SCANCODE_KP_1)) held |= InputState::kDown | InputState::kLeft;
        if (down(SDL_SCANCODE_KP_3)) held |= InputState::kDown | InputState::kRight;
        if (down(SDL_SCANCODE_ESCAPE)) held |= InputState::kEscape;
        if (down(SDL_SCANCODE_LCTRL) || down(SDL_SCANCODE_RCTRL) || down(SDL_SCANCODE_C)) held |= InputState::kSkip;
        if (down(SDL_SCANCODE_Q)) held |= InputState::kQ;
        if (down(SDL_SCANCODE_S)) held |= InputState::kS;
        if (down(SDL_SCANCODE_P) || down(SDL_SCANCODE_HOME)) held |= InputState::kPause;
        if (down(SDL_SCANCODE_RETURN) || down(SDL_SCANCODE_KP_ENTER)) held |= InputState::kEnter;
        if (down(SDL_SCANCODE_D)) held |= InputState::kD;
        if (down(SDL_SCANCODE_R)) held |= InputState::kR;
        if (down(SDL_SCANCODE_F10)) held |= InputState::kF10;
    }
#endif
    session.host_input = held;
}

bool Application::initialize(const char* archive_path) {
    if (initialized) return true;
    if (!read_file(archive_path, archive_bytes)) { failure = std::string("cannot read ") + archive_path; return false; }
    if (!session.open(archive_bytes.data(), u32(archive_bytes.size()))) { failure = session.failure; return false; }
    // 0x44af80 demo 路径相对游戏目录：demo/demoN.rpy（与 th16.dat 同级）
    if (const char* slash = std::strrchr(archive_path, '/')) {
        session.demo_directory.assign(archive_path, slash);
    } else if (const char* slash = std::strrchr(archive_path, '\\')) {
        session.demo_directory.assign(archive_path, slash);
    } else {
        session.demo_directory = ".";
    }
    graphics.animations = &session.animations;
    if (!graphics.initialize()) { failure = graphics.error; return false; }

    session.attach_device(&graphics);
    session.on_loaded = [this](AnmResource& res) -> bool {
        if (!graphics.preload(res)) { failure = graphics.error; session.failure = graphics.error; return false; }
        return true;
    };

    if (!session.begin_boot()) { failure = session.failure; return false; }
    initialized = true;
    return true;
}

bool Application::step() {
    if (!initialized) return false;
    sample_input();
    if (!session.logic_step()) { failure = session.failure; return false; }
    fonts.flush(graphics, session.animations, session.text_into_texture);
    graphics.present();
    ++frames;
    if (!graphics.error.empty()) failure = graphics.error;
    return failure.empty();
}

bool Application::step_at(double wall_seconds) {
    if (!initialized) return false;
    const u32 steps = clock.begin_frame(wall_seconds);
    sample_input();
    session.wall_clock_seconds = wall_seconds;
    for (u32 i = 0; i < steps; ++i) {
        if (!session.logic_step()) { failure = session.failure; return false; }
        fonts.flush(graphics, session.animations, session.text_into_texture);
    }
    graphics.present();
    ++frames;
    if (!graphics.error.empty()) failure = graphics.error;
    return failure.empty();
}

}  // namespace th16::sdl
