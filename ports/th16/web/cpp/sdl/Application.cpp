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

// D3DTADDRESS → 共享后端
touhou::graphics::Address address_mode(u8 d3d) {
    using touhou::graphics::Address;
    switch (d3d) {
    case 2: return Address::Mirror;
    case 3: return Address::Clamp;
    default: return Address::Repeat;
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
    // 0x45ba80 缺省状态：ZENABLE=1 ZFUNC=ALWAYS、ALPHATEST ref=1 GREATEREQUAL、纹理 × 颜色调制
    p.depthTest = true; p.depthWrite = false; p.depthCompare = Compare::Always;
    p.alphaTest = true; p.alphaReference = 1; p.alphaCompare = Compare::GreaterEqual;
    p.color.operation = p.alpha.operation = ColorOperation::Multiply;
    p.color.first = p.alpha.first = {ArgumentSource::Texture};
    p.color.second = p.alpha.second = {ArgumentSource::Diffuse};
    graphics.backend.state.viewport = {0, 0, 640, 480, 0, 1};
    graphics.clear(0xff000000);

    // 0x409f90（绘制链优先级 3）：关卡背景——原版画进场地 RT 再合成；此处直接画到屏幕场地矩形
    if (session.game_live) {
        StagePass pass;
        if (!renderer.emit_stage(session.animations, session.game.stage, pass)) { failure = renderer.error; return; }
        if (pass.active) {
            graphics.clear_rect(pass.clear_color, pass.clear_rect);
            graphics.backend.state.viewport = {u32(pass.clear_rect[0]), u32(pass.clear_rect[1]),
                                               u32(pass.clear_rect[2] - pass.clear_rect[0]),
                                               u32(pass.clear_rect[3] - pass.clear_rect[1]), 0, 1};
            p.fogMode = FogMode::Linear;
            p.fogColor = pass.fog_color;
            p.fogNear = pass.fog_near;
            p.fogFar = pass.fog_far;
            for (const AnmQuad& q : pass.quads) {
                apply_blend(p, q.blend);
                p.minFilter = p.magFilter = q.nearest ? Filter::Nearest : Filter::Linear;
                p.addressU = address_mode(q.address_u);
                p.addressV = address_mode(q.address_v);
                p.depthTest = q.depth_test;
                p.depthWrite = q.depth_write;
                p.fog = q.fog;
                const GraphicsDevice::FogVertex quad[4] = {
                    {q.x0, q.y0, q.z[0], q.rhw[0], q.color0, u32(q.fog_factor[0]) << 24, q.u0, q.v0},
                    {q.x1, q.y1, q.z[1], q.rhw[1], q.color1, u32(q.fog_factor[1]) << 24, q.u1, q.v1},
                    {q.x2, q.y2, q.z[2], q.rhw[2], q.color2, u32(q.fog_factor[2]) << 24, q.u2, q.v2},
                    {q.x3, q.y3, q.z[3], q.rhw[3], q.color3, u32(q.fog_factor[3]) << 24, q.u3, q.v3},
                };
                graphics.screen_quad_fog(q.texture, quad);
            }
            graphics.backend.state.viewport = {0, 0, 640, 480, 0, 1};
            p.fog = false;
            p.depthTest = true;
        }
    }

    std::vector<AnmQuad> quads;
    if (!renderer.emit_all(session.animations, quads)) { failure = renderer.error; return; }
    for (const AnmQuad& q : quads) {
        apply_blend(p, q.blend);
        p.minFilter = p.magFilter = q.nearest ? Filter::Nearest : Filter::Linear;
        p.addressU = address_mode(q.address_u);
        p.addressV = address_mode(q.address_v);
        // 0x45ba80：ZENABLE 开、ZFUNC=D3DCMP_ALWAYS；Z 写由各模式决定（0x43c530/0x43c570）
        p.depthWrite = q.depth_write;
        const GraphicsDevice::Vertex quad[4] = {
            {q.x0, q.y0, q.z[0], q.rhw[0], q.color0, q.u0, q.v0},
            {q.x1, q.y1, q.z[1], q.rhw[1], q.color1, q.u1, q.v1},
            {q.x2, q.y2, q.z[2], q.rhw[2], q.color2, q.u2, q.v2},
            {q.x3, q.y3, q.z[3], q.rhw[3], q.color3, q.u3, q.v3},
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
