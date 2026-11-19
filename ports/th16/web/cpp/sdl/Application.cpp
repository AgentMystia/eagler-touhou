#include "Application.hpp"

#include "../game/ScancodeMap.hpp"
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

// 0x401d50 键盘位（固定键位，Key Config 不改键盘）：Z 射击 / X 炸弹 / Shift 低速 / Ctrl 跳过 / C 跳过+季节解放；
// 方向键与小键盘；Esc / Q / S / P·Home / Enter / D / R / F10。手柄：0x401c30 按钮快照交给 GameSession（0x4018e0 按表合成），
// 摇杆轴按原版“中心 ± 1/4 量程”阈值出方向位；POV 帽原版不读，这里一并映射到方向（现代手柄十字键多报为帽）
void Application::sample_input() {
    u32 held = 0;
    u32 buttons = 0;
#if defined(__EMSCRIPTEN__) || defined(TH16_HAS_SDL)
    int n = 0;
    const bool* keys = SDL_GetKeyboardState(&n);
    if (keys != nullptr) {
        auto down = [&](SDL_Scancode s) { return s != SDL_SCANCODE_UNKNOWN && s < n && keys[s]; };
        if (down(SDL_SCANCODE_Z)) held |= InputState::kShot;
        if (down(SDL_SCANCODE_X)) held |= InputState::kBomb;
        if (down(SDL_SCANCODE_LSHIFT) || down(SDL_SCANCODE_RSHIFT)) held |= InputState::kSlow;
        if (down(SDL_SCANCODE_LCTRL) || down(SDL_SCANCODE_RCTRL)) held |= InputState::kSkip;
        if (down(SDL_SCANCODE_C)) held |= InputState::kSkip | InputState::kRelease;
        if (down(SDL_SCANCODE_UP) || down(SDL_SCANCODE_KP_8)) held |= InputState::kUp;
        if (down(SDL_SCANCODE_DOWN) || down(SDL_SCANCODE_KP_2)) held |= InputState::kDown;
        if (down(SDL_SCANCODE_LEFT) || down(SDL_SCANCODE_KP_4)) held |= InputState::kLeft;
        if (down(SDL_SCANCODE_RIGHT) || down(SDL_SCANCODE_KP_6)) held |= InputState::kRight;
        if (down(SDL_SCANCODE_KP_7)) held |= InputState::kUp | InputState::kLeft;
        if (down(SDL_SCANCODE_KP_9)) held |= InputState::kUp | InputState::kRight;
        if (down(SDL_SCANCODE_KP_1)) held |= InputState::kDown | InputState::kLeft;
        if (down(SDL_SCANCODE_KP_3)) held |= InputState::kDown | InputState::kRight;
        if (down(SDL_SCANCODE_ESCAPE)) held |= InputState::kEscape;
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
    }
    if (joystick == nullptr) {
        int count = 0;
        if (SDL_JoystickID* ids = SDL_GetJoysticks(&count)) {
            if (count > 0) joystick = SDL_OpenJoystick(ids[0]);
            SDL_free(ids);
        }
    } else if (!SDL_JoystickConnected(joystick)) {
        SDL_CloseJoystick(joystick);
        joystick = nullptr;
    }
    if (joystick != nullptr) {
        const int button_count = SDL_GetNumJoystickButtons(joystick);
        for (int i = 0; i < button_count && i < 32; ++i) {
            if (SDL_GetJoystickButton(joystick, i)) buttons |= 1u << i;
        }
        const int axes = SDL_GetNumJoystickAxes(joystick);
        const i32 x = axes > 0 ? SDL_GetJoystickAxis(joystick, 0) : 0;
        const i32 y = axes > 1 ? SDL_GetJoystickAxis(joystick, 1) : 0;
        if (x < -16384) held |= InputState::kLeft;
        if (x > 16384) held |= InputState::kRight;
        if (y < -16384) held |= InputState::kUp;
        if (y > 16384) held |= InputState::kDown;
        const u8 hat = SDL_GetNumJoystickHats(joystick) > 0 ? SDL_GetJoystickHat(joystick, 0) : 0;
        if (hat & SDL_HAT_UP) held |= InputState::kUp;
        if (hat & SDL_HAT_DOWN) held |= InputState::kDown;
        if (hat & SDL_HAT_LEFT) held |= InputState::kLeft;
        if (hat & SDL_HAT_RIGHT) held |= InputState::kRight;
    }
#endif
    session.host_input = held;
    session.pad_buttons = buttons;
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
    // 0x4497e0：scoreth16.dat 在游戏目录（0x479a65 切到该目录后相对路径读写）；0x43c050：th16.cfg 同目录
    session.score_path = session.replay_directory() + "/scoreth16.dat";
    session.config_path = session.replay_directory() + "/th16.cfg";
    session.load_config();
    graphics.animations = &session.animations;
    if (!graphics.initialize()) { failure = graphics.error; return false; }

    session.attach_device(&graphics);
#if defined(__EMSCRIPTEN__) || defined(TH16_HAS_SDL)
    // 0x401c30 手柄：无手柄或子系统不可用时不致命，按钮位图保持 0
    if (!SDL_InitSubSystem(SDL_INIT_JOYSTICK)) std::fprintf(stderr, "th16: joystick disabled: %s\n", SDL_GetError());
#endif
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
