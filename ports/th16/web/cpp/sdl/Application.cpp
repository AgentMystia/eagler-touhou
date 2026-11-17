#include "Application.hpp"

#include <cstdio>

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

bool Application::initialize(const char* archive_path) {
    if (initialized) return true;
    if (!read_file(archive_path, archive_bytes)) { failure = std::string("cannot read ") + archive_path; return false; }
    if (!archive.open(archive_bytes.data(), u32(archive_bytes.size()))) { failure = archive.error; return false; }
    if (!graphics.initialize()) { failure = graphics.error; return false; }
    if (!load_anm("sig.anm", sig)) return false;
    initialized = true;
    return true;
}

// sig.anm script0/script1 的稳态（time ≥ 10）：sprite0 @ (0,0)、sprite1 @ (1024,0)，
// ins_431(1) = 1280x960 坐标系 → 逻辑 640x480 需 ×0.5；ins_408 淡入结束后为纯白。
void Application::draw_boot_frame() {
    using namespace touhou::graphics;
    auto& p = graphics.backend.pipeline();
    p = {};
    p.depthTest = false; p.depthWrite = false; p.blend = true;
    p.sourceBlend = BlendFactor::SourceAlpha; p.destinationBlend = BlendFactor::InverseSourceAlpha;
    p.color.operation = p.alpha.operation = ColorOperation::Multiply;
    p.color.first = p.alpha.first = {ArgumentSource::Texture};
    p.color.second = p.alpha.second = {ArgumentSource::Diffuse};
    p.addressU = p.addressV = Address::Clamp;
    p.minFilter = p.magFilter = Filter::Linear;
    graphics.backend.state.viewport = {0, 0, 640, 480, 0, 1};
    graphics.clear(0xff000000);
    const u32 white = 0xffffffff;
    for (u32 index = 0; index < sig.entries.size(); ++index) {
        const auto& entry = sig.entries[index];
        const u32 handle = graphics.texture(sig, index);
        if (!handle || entry.sprites.empty()) continue;
        const auto& sprite = entry.sprites[0];
        const float tw = float(entry.texture.width), th = float(entry.texture.height);
        const float x0 = (float(entry.x) + sprite.x) * 0.5f, y0 = (float(entry.y) + sprite.y) * 0.5f;
        const float x1 = x0 + sprite.w * 0.5f, y1 = y0 + sprite.h * 0.5f;
        const float u0 = sprite.x / tw, v0 = sprite.y / th, u1 = (sprite.x + sprite.w) / tw, v1 = (sprite.y + sprite.h) / th;
        const GraphicsDevice::Vertex quad[4] = {{x0, y0, 0, 1, white, u0, v0}, {x1, y0, 0, 1, white, u1, v0},
                                                {x0, y1, 0, 1, white, u0, v1}, {x1, y1, 0, 1, white, u1, v1}};
        graphics.screen_quad(handle, quad);
    }
    graphics.present();
}

bool Application::step() {
    if (!initialized) return false;
    draw_boot_frame();
    ++frames;
    return true;
}

}  // namespace th16::sdl
