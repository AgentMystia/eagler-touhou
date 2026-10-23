// 图形设备（th15 GraphicsDevice 裁剪：ZunGraphics 语义状态 → portable/sdl
// Renderer；离屏捕获/重采样/PNG 上传与高刷插值（PresentationFrame）延后，
// ui-base.md §三）。Profile 10 为精度档，不是 Windows 图形 API。
#pragma once

#include "../game/ZunGraphics.hpp"
#include "../game/TextureImage.hpp"
#include "../game/DrawCapture.hpp"
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

    // ---- DrawCapture 影子（web 语义清单确定性导出；M0 门禁奠基）----
    // 运行时激活（th13_debug_capture_begin）：逐调用转发语义状态、逐提交
    // 转发 op，与 oracle/harness 候选同 schema（draw-contract.md）。状态策略：
    // ①pipeline 在提交时从 backend.state 整体快照——pipeline() 按引用返回的
    // 语义对影子的等价物（引用无法双写；backend.state.pipeline 是唯一真值，
    // 已核无旁路写点）；②矩阵/布局/视口/纹理绑定按虚调用转发（全部状态写
    // 均经虚接口）；③帧界 = present()（clear_device_texture + end_frame +
    // 次帧 begin_frame），帧尾 stale 层口径由 SceneDisplay/render_loading 的
    // capture_stale_layer(56) 定位（harness render_frame 同点位）。
    // 精灵层号钩子（AnmRenderer TH13_DRAW_SPRITE）需捕获构建
    // （TH13_WEB_CAPTURE=1 → -DTH13_DRAW_CAPTURE）；非捕获构建激活时层号
    // 恒 -100，仅供粗诊断。
    DrawCapture capture_shadow;
    bool capture_begin(const char* path);
    void capture_finish() noexcept;
    void capture_stale_layer(i32 layer) noexcept {
        if (capture_on_) capture_shadow.set_stale_layer(layer);
    }
    bool capture_active() const noexcept { return capture_on_; }

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
    void bind_texture(u32 handle) override {
        backend.state.texture = handle;
        if (capture_on_) capture_shadow.bind_texture(capture_texture(handle));
    }
    touhou::graphics::PipelineState& pipeline() override { return backend.pipeline(); }
    void set_layout(touhou::graphics::VertexLayout layout) override {
        backend.state.layout = touhou::graphics::attributes(layout);
        if (capture_on_) capture_shadow.set_layout(layout);
    }
    void set_matrix(touhou::graphics::MatrixKind kind, const Matrix4& matrix) override {
        backend.transform(kind, matrix.m);
        if (capture_on_) capture_shadow.set_matrix(kind, matrix);
    }
    void triangles(u32 count, const void* data, u32 stride) override {
        backend.draw_batch(count, data, stride);
        if (capture_on_) {
            capture_submit();
            capture_shadow.triangles(count, data, stride);
        }
    }
    void primitives(touhou::graphics::Topology kind, u32 count, const void* data,
                    u32 stride) override {
        backend.draw(kind, count, data, stride);
        if (capture_on_) {
            capture_submit();
            capture_shadow.primitives(kind, count, data, stride);
        }
    }
    bool select_target(const AnmResource*, u32) override;
    bool clear_target(u32, const GraphicsViewport*) override;
    bool clear_depth(const GraphicsViewport*) override;
    void set_viewport(const GraphicsViewport& v) override {
        viewport(v);
        if (capture_on_) capture_shadow.set_viewport(v);
    }
    void viewport(const GraphicsViewport& v) {
        backend.viewport({v.x, v.y, v.width, v.height, v.near_depth, v.far_depth});
    }
    void clear(u32 color, bool clear_depth = true) {
        backend.clear(clear_depth ? 3 : 1, color, 1, 0);
    }
    void present() {
        backend.present(screen);
        if (capture_on_) {
            capture_shadow.clear_device_texture();  // 0x45d2f0:49 帧尾 SetTexture(0,0)
            capture_shadow.end_frame();
            capture_shadow.begin_frame(++capture_frames_);
        }
    }

private:
    bool capture_on_ = false;
    u32 capture_frames_ = 0;
    std::map<u32, u32> capture_handles_;  // 设备句柄 → 影子句柄（texture() 登记）
    // 提交时快照：backend.state.pipeline → 影子（pipeline() 引用语义的等价物）。
    void capture_submit() { capture_shadow.pipeline() = backend.state.pipeline; }
    // 影子句柄解析：未登记（捕获开始前装载/绑定）按 0 = tex null 可见暴露。
    u32 capture_texture(u32 handle) {
        const auto it = capture_handles_.find(handle);
        return it == capture_handles_.end() ? 0 : it->second;
    }
};

}  // namespace th13::sdl
