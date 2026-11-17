#include "GraphicsDevice.hpp"

#include <SDL3/SDL.h>

namespace th16::sdl {

touhou::sdl::Surface GraphicsDevice::resolve(void* owner, u32 id) {
    auto& device = *static_cast<GraphicsDevice*>(owner);
    auto it = device.textures.find(id);
    if (it == device.textures.end()) return {};
    auto& t = it->second;
    return {id, t.width, t.height, t.format, t.width * 4, t.pixels.data(), u32(t.pixels.size()), t.revision, t.renderScale};
}

bool GraphicsDevice::initialize() {
    using namespace touhou::graphics;
    for (u32 id : {screen, depth}) {
        auto& t = textures[id];
        t.renderScale = render_scale;
        t.identity = id == screen ? "device:screen" : "device:depth";
        t.width = 640;
        t.height = 480;
        t.format = id == screen ? PixelFormat::Bgra8 : PixelFormat::Depth16;
        if (id == screen) t.pixels.resize(640 * 480 * 4);
    }
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
    if (!backend.initialize()) {
        error = backend.error();
        return false;
    }
    SDL_SetWindowTitle(SDL_GL_GetCurrentWindow(), "Touhou 16");
    SDL_SetWindowSize(SDL_GL_GetCurrentWindow(), 640 * render_scale, 480 * render_scale);
    backend.state.target = screen;
    backend.state.depth = depth;
    // 真机 D3D 设备初始 TFACTOR = 0；共享缺省 0xffffffff 是 th15 的显式配置惰性缺省。
    backend.pipeline().textureFactor = 0;
    return true;
}

bool GraphicsDevice::preload(const AnmResource& file) {
    if (resources.count(&file)) return true;
    std::vector<u32> handles;
    handles.reserve(file.entries.size());
    for (u32 index = 0; index < file.entries.size(); ++index) {
        const auto& entry = file.entries[index];
        if (!entry.texture.present()) { handles.push_back(0); continue; }
        Texture t;
        t.width = entry.texture.width;
        t.height = entry.texture.height;
        t.pixels = entry.texture.bgra;
        t.identity = "anm:" + file.name + ":entry:" + std::to_string(index);
        const u32 id = next_handle++;
        textures.emplace(id, std::move(t));
        handles.push_back(id);
        backend.prepare(id);
    }
    resources.emplace(&file, std::move(handles));
    return true;
}

void GraphicsDevice::unload(const AnmResource& file) {
    auto it = resources.find(&file);
    if (it == resources.end()) return;
    for (u32 id : it->second) if (id) { backend.release(id); textures.erase(id); }
    resources.erase(it);
}

u32 GraphicsDevice::texture(const AnmResource& file, u32 entry) const {
    auto it = resources.find(&file);
    if (it == resources.end() || entry >= it->second.size()) return 0;
    return it->second[entry];
}

void GraphicsDevice::screen_quad(u32 texture, const Vertex quad[4]) {
    using namespace touhou::graphics;
    auto& s = backend.state;
    s.texture = texture;
    s.layout = attributes(VertexLayout::ScreenColorUv);
    backend.draw(Topology::Strip, 2, quad, sizeof(Vertex));
}

void GraphicsDevice::clear(u32 color, bool with_depth) {
    backend.clear(with_depth ? 3 : 1, color, 1.0f, 0);
}

void GraphicsDevice::present() { backend.present(screen); }

}  // namespace th16::sdl
