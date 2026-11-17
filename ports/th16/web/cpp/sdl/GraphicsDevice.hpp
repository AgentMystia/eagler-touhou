// th16 设备层：ANM 纹理句柄 → 共享语义 GLES3 后端（portable/sdl/Renderer，与 th13/th15 同源不分叉）。
// 原版 0x46cdd0 族把 THTX 交给 D3D9 纹理；这里 CPU 解码为 BGRA8 后由 Renderer::surface() 惰性上传。
#pragma once
#include "../game/AnmResource.hpp"
#include "../game/Types.hpp"
#include "../../../portable/sdl/Renderer.hpp"

#include <map>
#include <string>
#include <vector>

namespace th16::sdl {

class GraphicsDevice {
    struct Texture {
        u32 width = 0, height = 0, revision = 0, renderScale = 1;
        touhou::graphics::PixelFormat format = touhou::graphics::PixelFormat::Bgra8;
        std::vector<u8> pixels;
        std::string identity;
    };
    std::map<u32, Texture> textures;
    std::map<const AnmResource*, std::vector<u32>> resources;
    u32 next_handle = 3;
    static touhou::sdl::Surface resolve(void*, u32);

public:
    static constexpr u32 screen = 1, depth = 2;
    touhou::sdl::Renderer backend{10, resolve, this};
    std::string error;
    u32 render_scale = 1;

    bool initialize();
    // 为 AnmResource 的每个 entry 建纹理句柄；无纹理的 entry 句柄为 0。
    bool preload(const AnmResource&);
    void unload(const AnmResource&);
    u32 texture(const AnmResource&, u32 entry) const;

    struct Vertex { float x, y, z, w; u32 color; float u, v; };
    // 屏幕空间（640x480 逻辑像素）带色纹理四边形：Strip 2 三角形。
    void screen_quad(u32 texture, const Vertex quad[4]);
    // 带镜面色（specular.a = 顶点雾因子，D3D XYZRHW 语义）的屏幕四边形
    struct FogVertex { float x, y, z, w; u32 color; u32 specular; float u, v; };
    void screen_quad_fog(u32 texture, const FogVertex quad[4]);
    void clear(u32 color, bool depth = true);
    // 矩形清屏（D3D Clear 带 rect；x0,y0,x1,y1 屏幕像素）
    void clear_rect(u32 color, const i32 rect[4], bool depth = true);
    void present();
    void flush() { backend.flush(); }
};

}  // namespace th16::sdl
