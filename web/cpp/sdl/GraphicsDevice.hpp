// 图形设备（th15 GraphicsDevice 裁剪：ZunGraphics 语义状态 → portable/sdl
// Renderer；离屏捕获/重采样/PNG 上传与高刷插值（PresentationFrame）延后，
// ui-base.md §三）。Profile 10 为精度档，不是 Windows 图形 API。
#pragma once

#include "../game/ZunGraphics.hpp"
#include "../game/TextureImage.hpp"
#include "../../../portable/sdl/Renderer.hpp"

#include <map>

namespace th13::sdl {

class GraphicsDevice final : public ZunGraphics {
    struct Texture {
        TextureImage image;
        u32 revision = 0, renderScale = 1;
    };
    std::map<u32, Texture> textures;
    std::map<const AnmResource*, std::vector<u32>> resources;
    u32 next_handle = 3, startup_credit = 0;
    static touhou::sdl::Surface resolve(void*, u32);

public:
    touhou::sdl::Renderer backend{10, resolve, this};
    static constexpr u32 screen = 1, depth = 2;
    std::string error;
    u32 render_scale = 1;
    // 表现插值通道占位（PresentationFrame 分期接入；当前直接透传）。
    struct PresentationStub {
        bool enabled = false;
        void begin() {}
        void finish() {}
        void reset() {}
    } presentation;

    void presentation_offset(Vec2) override {}
    void presentation_range(const AnmVm&, u32, u32, u32 = 0) override {}
    void presentation_camera(bool) override {}

    bool initialize();
    void draw_startup_branding(u32 tint);
    void release_startup_branding();
    bool preload(AnmResource&, bool low_color = false);
    void unload(const AnmResource&);
    u32 texture(const AnmResource&, u32) override;
    TextureImage* pixels(u32 handle) {
        auto it = textures.find(handle);
        return it == textures.end() ? nullptr : &it->second.image;
    }
    void changed(u32 handle, std::array<u32, 4> region = {}) {
        auto it = textures.find(handle);
        if (it != textures.end()) {
            backend.flush();
            backend.changed(handle, it->second.revision, region);
            ++it->second.revision;
        }
    }
    void bind_texture(u32 handle) override { backend.state.texture = handle; }
    touhou::graphics::PipelineState& pipeline() override { return backend.pipeline(); }
    void set_layout(touhou::graphics::VertexLayout layout) override {
        backend.state.layout = touhou::graphics::attributes(layout);
    }
    void set_matrix(touhou::graphics::MatrixKind kind, const Matrix4& matrix) override {
        backend.transform(kind, matrix.m);
    }
    void triangles(u32 count, const void* data, u32 stride) override {
        backend.draw_batch(count, data, stride);
    }
    void primitives(touhou::graphics::Topology kind, u32 count, const void* data,
                    u32 stride) override {
        backend.draw(kind, count, data, stride);
    }
    bool select_target(const AnmResource*, u32) override;
    bool clear_target(u32, const GraphicsViewport*) override;
    bool clear_depth(const GraphicsViewport*) override;
    void set_viewport(const GraphicsViewport& v) override { viewport(v); }
    void viewport(const GraphicsViewport& v) {
        backend.viewport({v.x, v.y, v.width, v.height, v.near_depth, v.far_depth});
    }
    void clear(u32 color, bool clear_depth = true) {
        backend.clear(clear_depth ? 3 : 1, color, 1, 0);
    }
    void present() { backend.present(screen); }
};

}  // namespace th13::sdl
