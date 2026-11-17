#include "Application.hpp"

#include <cstdio>

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

void apply_blend(touhou::graphics::PipelineState& p, int blend) {
    using namespace touhou::graphics;
    p.blend = true;
    p.blendEquation = BlendEquation::Add;
    switch (blend) {
    case 1: p.sourceBlend = BlendFactor::SourceAlpha; p.destinationBlend = BlendFactor::One; break;
    case 2: p.sourceBlend = BlendFactor::SourceAlpha; p.destinationBlend = BlendFactor::One; p.blendEquation = BlendEquation::ReverseSubtract; break;
    case 3: p.blend = false; p.sourceBlend = BlendFactor::One; p.destinationBlend = BlendFactor::Zero; break;
    case 4: p.sourceBlend = BlendFactor::InverseDestinationColor; p.destinationBlend = BlendFactor::InverseSourceColor; break;
    case 5: p.sourceBlend = BlendFactor::DestinationColor; p.destinationBlend = BlendFactor::Zero; break;
    case 6: p.sourceBlend = BlendFactor::InverseSourceColor; p.destinationBlend = BlendFactor::InverseSourceAlpha; break;
    case 7: p.sourceBlend = BlendFactor::DestinationAlpha; p.destinationBlend = BlendFactor::InverseDestinationAlpha; break;
    case 8: p.sourceBlend = BlendFactor::SourceAlpha; p.destinationBlend = BlendFactor::One; p.blendEquation = BlendEquation::Minimum; break;
    case 9: p.sourceBlend = BlendFactor::SourceAlpha; p.destinationBlend = BlendFactor::One; p.blendEquation = BlendEquation::Maximum; break;
    default: p.sourceBlend = BlendFactor::SourceAlpha; p.destinationBlend = BlendFactor::InverseSourceAlpha; break;
    }
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
    if (!graphics.initialize()) { failure = graphics.error; return false; }

    renderer.window_scale = 1.0f;
    renderer.lookup = [this](int file_slot, int entry_index) -> u32 {
        const AnmFile* f = session.animations.file(file_slot);
        if (f == nullptr || f->resource == nullptr) return 0;
        return graphics.texture(*f->resource, u32(entry_index));
    };
    session.on_loaded = [this](AnmResource& res) -> bool {
        if (!graphics.preload(res)) { failure = graphics.error; session.failure = graphics.error; return false; }
        return true;
    };

    if (!session.begin_boot()) { failure = session.failure; return false; }
    initialized = true;
    return true;
}

void Application::draw_frame() {
    using namespace touhou::graphics;
    auto& p = graphics.backend.pipeline();
    p = {};
    p.depthTest = false; p.depthWrite = false;
    p.color.operation = p.alpha.operation = ColorOperation::Multiply;
    p.color.first = p.alpha.first = {ArgumentSource::Texture};
    p.color.second = p.alpha.second = {ArgumentSource::Diffuse};
    p.addressU = p.addressV = Address::Clamp;
    graphics.backend.state.viewport = {0, 0, 640, 480, 0, 1};
    graphics.clear(0xff000000);

    std::vector<AnmQuad> quads;
    if (!renderer.emit_all(session.animations, quads)) { failure = renderer.error; return; }
    for (const AnmQuad& q : quads) {
        apply_blend(p, q.blend);
        p.minFilter = p.magFilter = q.nearest ? Filter::Nearest : Filter::Linear;
        const GraphicsDevice::Vertex quad[4] = {
            {q.x0, q.y0, 0, 1, q.color0, q.u0, q.v0},
            {q.x1, q.y1, 0, 1, q.color1, q.u1, q.v1},
            {q.x2, q.y2, 0, 1, q.color2, q.u2, q.v2},
            {q.x3, q.y3, 0, 1, q.color3, q.u3, q.v3},
        };
        graphics.screen_quad(q.texture, quad);
    }
    graphics.present();
}

bool Application::step() {
    if (!initialized) return false;
    sample_input();
    if (!session.logic_step()) { failure = session.failure; return false; }
    draw_frame();
    ++frames;
    return failure.empty();
}

bool Application::step_at(double wall_seconds) {
    if (!initialized) return false;
    const u32 steps = clock.begin_frame(wall_seconds);
    sample_input();
    for (u32 i = 0; i < steps; ++i) {
        if (!session.logic_step()) { failure = session.failure; return false; }
    }
    draw_frame();
    ++frames;
    return failure.empty();
}

}  // namespace th16::sdl
