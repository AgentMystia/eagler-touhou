#include "Application.hpp"

#include "../game/KeyBindMap.hpp"
#include "../game/TitleScene.hpp"

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

// 0x401d50 键盘位：可配置 5 键走 GameGlobals::key_binds（DI 扫描码）；其余固定
void Application::sample_input() {
    u32 held = 0;
#if defined(__EMSCRIPTEN__) || defined(TH16_HAS_SDL)
    int n = 0;
    const bool* keys = SDL_GetKeyboardState(&n);
    if (keys != nullptr) {
        auto down = [&](SDL_Scancode s) { return s != SDL_SCANCODE_UNKNOWN && s < n && keys[s]; };
        auto down_di = [&](i16 di) { return down(di_to_sdl(di)); };

        // 可配置：Shot / Bomb / Slow / Skip / Release
        for (int i = 0; i < kConfigurableBinds; ++i) {
            if (down_di(session.globals.key_binds[i])) held |= key_bind_bit(i);
        }
        // 方向与菜单固定键（原版不在 Key Config 五项内）
        if (down(SDL_SCANCODE_UP) || down(SDL_SCANCODE_KP_8)) held |= InputState::kUp;
        if (down(SDL_SCANCODE_DOWN) || down(SDL_SCANCODE_KP_2)) held |= InputState::kDown;
        if (down(SDL_SCANCODE_LEFT) || down(SDL_SCANCODE_KP_4)) held |= InputState::kLeft;
        if (down(SDL_SCANCODE_RIGHT) || down(SDL_SCANCODE_KP_6)) held |= InputState::kRight;
        if (down(SDL_SCANCODE_KP_7)) held |= InputState::kUp | InputState::kLeft;
        if (down(SDL_SCANCODE_KP_9)) held |= InputState::kUp | InputState::kRight;
        if (down(SDL_SCANCODE_KP_1)) held |= InputState::kDown | InputState::kLeft;
        if (down(SDL_SCANCODE_KP_3)) held |= InputState::kDown | InputState::kRight;
        if (down(SDL_SCANCODE_ESCAPE)) held |= InputState::kEscape;
        if (down(SDL_SCANCODE_RCTRL)) held |= InputState::kSkip;  // 右 Ctrl 始终可 Skip（左 Ctrl 走绑定）
        if (down(SDL_SCANCODE_Q)) held |= InputState::kQ;
        if (down(SDL_SCANCODE_S)) held |= InputState::kS;
        if (down(SDL_SCANCODE_P) || down(SDL_SCANCODE_HOME)) held |= InputState::kPause;
        if (down(SDL_SCANCODE_RETURN) || down(SDL_SCANCODE_KP_ENTER)) held |= InputState::kEnter;
        if (down(SDL_SCANCODE_D)) held |= InputState::kD;
        if (down(SDL_SCANCODE_R)) held |= InputState::kR;
        if (down(SDL_SCANCODE_F10)) held |= InputState::kF10;

        std::memset(session.keyboard_state, 0, sizeof(session.keyboard_state));
        for (int i = 0; i < n && i < 512; ++i) {
            if (!keys[i]) continue;
            const i16 di = sdl_to_di(SDL_Scancode(i));
            if (di >= 0 && di < 256) session.keyboard_state[di] = 0x80;
        }

        // Key Config 捕获：首个新按下的可映射键 → pending_scancode（DI）
        TitleScene& title = session.title;
        if (title.live() && title.screen == TitleScene::kKeyConfig && title.key_bind_slot >= 0 &&
            title.pending_scancode < 0) {
            static bool prev[512] = {};
            for (int i = 0; i < n && i < 512; ++i) {
                if (keys[i] && !prev[i]) {
                    const i16 di = sdl_to_di(SDL_Scancode(i));
                    if (di >= 0) {
                        title.pending_scancode = di;
                        break;
                    }
                }
            }
            for (int i = 0; i < n && i < 512; ++i) prev[i] = keys[i];
        }
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
    // 0x4497e0：scoreth16.dat 在游戏目录（0x479a65 切到该目录后相对路径读写）
    session.score_path = session.replay_directory() + "/scoreth16.dat";
    graphics.animations = &session.animations;
    if (!graphics.initialize()) { failure = graphics.error; return false; }

    session.attach_device(&graphics);
    // 原版 SoundManager（DAT_004d9e10）的 DirectSound 侧：thbgm.dat 与 th16.dat 同目录（0x43adc0 以 "thbgm.dat" 打开）
    audio.thbgm_path = session.demo_directory + "/thbgm.dat";
    audio.music_directory = session.demo_directory + "/music";
    audio.music = &session.music;
    // 打不开音频时逻辑（命令队列 / 音效队列）照常推进，只是无输出——与 harness 同口径，逐帧行为不变
    if (audio.initialize(session.archive())) {
        session.sound.output = &audio.sfx_output();
        session.music.output = &audio.music_output();
    } else {
        std::fprintf(stderr, "th16: audio disabled: %s\n", audio.error.c_str());
    }
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
    audio.pump();
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
    audio.pump();
    graphics.present();
    ++frames;
    if (!graphics.error.empty()) failure = graphics.error;
    return failure.empty();
}

}  // namespace th16::sdl
