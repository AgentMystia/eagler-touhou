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

bool Application::load_anm(const char* name, AnmResource& out) {
    std::vector<u8> bytes;
    if (!archive.read(name, bytes)) { failure = archive.error; return false; }
    if (!out.parse(bytes.data(), u32(bytes.size()), name)) { failure = std::string(name) + ": " + out.error; return false; }
    for (u32 i = 0; i < out.entries.size(); ++i) {
        const auto& e = out.entries[i];
        if (e.has_data || e.name.empty() || e.name[0] == '@') continue;
        std::vector<u8> external;
        if (!archive.read(e.name.c_str(), external)) { failure = archive.error; return false; }
        if (!out.attach_external_texture(i, external.data(), u32(external.size()))) { failure = out.error; return false; }
    }
    return graphics.preload(out);
}

bool Application::enter_boot() {
    // 0x407cb0 AsciiManager：ascii.anm → 槽 2
    if (!load_anm("ascii.anm", ascii)) return false;
    animations.load(AnmManager::kAsciiSlot, ascii);

    // 0x43adc0：sig.anm → 槽 1；0x43b300：spawn script0，layer -1
    if (!load_anm("sig.anm", sig)) return false;
    AnmFile* file = animations.load(1, sig);
    if (file == nullptr) { failure = "sig.anm load failed"; return false; }
    animations.spawn(*file, 0, -1);

    supervisor.boot_frames = 0;
    supervisor.current = SceneId::Boot;
    // 装载完成即启用更新链 → 0x43b290 请求标题
    supervisor.boot_init_finished();
    return true;
}

bool Application::enter_title_assets() {
    // 0x44ac70：title.anm → 槽 0x10，title_v.anm → 槽 0x11
    if (!load_anm("title.anm", title)) return false;
    if (!load_anm("title_v.anm", title_v)) return false;
    if (animations.load(0x10, title) == nullptr) { failure = "title.anm load failed"; return false; }
    if (animations.load(0x11, title_v) == nullptr) { failure = "title_v.anm load failed"; return false; }
    supervisor.title_assets_ready = true;
    supervisor.title_menu_live = false;
    return true;
}

bool Application::activate_title_menu() {
    // 0x44ac21：卸掉 sig.anm（槽 1）
    animations.kill_file(1);
    animations.tick_world();
    animations.unload(1);

    AnmFile* title_file = animations.file(0x10);
    AnmFile* title_v_file = animations.file(0x11);
    if (title_file == nullptr || title_v_file == nullptr) { failure = "title banks missing"; return false; }

    // 0x44b5f0 case 0/1 入口子集：logo 相关脚本 0x61 / 0x65 / 0，以及 title_v script0
    animations.spawn(*title_file, 0x61, -1);
    animations.spawn(*title_file, 0x65, -1);
    animations.spawn(*title_file, 0, -1);
    animations.spawn(*title_v_file, 0, -1);

    supervisor.title_menu_live = true;
    return true;
}

void Application::sample_input() {
    u32 held = 0;
#if defined(__EMSCRIPTEN__) || defined(TH16_HAS_SDL)
    int n = 0;
    const bool* keys = SDL_GetKeyboardState(&n);
    if (keys != nullptr) {
        auto down = [&](SDL_Scancode s) { return s < n && keys[s]; };
        if (down(SDL_SCANCODE_Z) || down(SDL_SCANCODE_RETURN) || down(SDL_SCANCODE_KP_ENTER)) held |= InputState::kShot;
        if (down(SDL_SCANCODE_X)) held |= InputState::kBomb;
        if (down(SDL_SCANCODE_LSHIFT) || down(SDL_SCANCODE_RSHIFT)) held |= InputState::kSlow;
        if (down(SDL_SCANCODE_ESCAPE)) held |= InputState::kCancel;
        if (down(SDL_SCANCODE_UP)) held |= InputState::kUp;
        if (down(SDL_SCANCODE_DOWN)) held |= InputState::kDown;
        if (down(SDL_SCANCODE_LEFT)) held |= InputState::kLeft;
        if (down(SDL_SCANCODE_RIGHT)) held |= InputState::kRight;
        if (down(SDL_SCANCODE_Q)) held |= InputState::kSkip;
    }
#endif
    supervisor.input.held = held;
    supervisor.input.commit();
}

bool Application::initialize(const char* archive_path) {
    if (initialized) return true;
    if (!read_file(archive_path, archive_bytes)) { failure = std::string("cannot read ") + archive_path; return false; }
    if (!archive.open(archive_bytes.data(), u32(archive_bytes.size()))) { failure = archive.error; return false; }
    if (!graphics.initialize()) { failure = graphics.error; return false; }

    renderer.window_scale = 1.0f;
    renderer.lookup = [this](int file_slot, int entry_index) -> u32 {
        const AnmFile* f = animations.file(file_slot);
        if (f == nullptr || f->resource == nullptr) return 0;
        return graphics.texture(*f->resource, u32(entry_index));
    };
    animations.game_speed = supervisor.game_speed;

    if (!enter_boot()) return false;
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
    if (!renderer.emit_all(animations, quads)) { failure = renderer.error; return; }
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

    SceneId leave = SceneId::Invalid, enter = SceneId::Invalid;
    if (supervisor.consume_switch(leave, enter)) {
        if (enter == SceneId::Title) {
            if (!enter_title_assets()) return false;
        }
    }

    // 启动幕绘制回调 0x43b300：帧计数 +1；标题未激活时仍跑启动幕 VM
    if (!supervisor.title_menu_live) {
        ++supervisor.boot_frames;
        if (supervisor.title_assets_ready && supervisor.boot_hold_elapsed()) {
            if (!activate_title_menu()) return false;
        }
    }

    animations.game_speed = supervisor.game_speed;
    animations.tick_world();
    draw_frame();
    ++frames;
    return failure.empty();
}

}  // namespace th16::sdl
