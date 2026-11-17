// th16 ANM 绘制——原版 0x468490（按 layer 链逐 VM 绘制）的屏幕四边形路径。
//
// 本文件覆盖启动幕 / 标题 / HUD 所需的 render mode：
//   0x465c40 轴对齐几何（mode 0/2）  0x4660b0 带旋转（mode 1/3）
//   0x465280 像素对齐、UV 滚动、颜色打包、提交 D3D DrawPrimitive（XYZRHW）
//   0x467410 mode 8：世界矩阵（精灵尺寸/256 × scale × 三轴旋转 × 平移）经相机 view/proj 投影，
//            ±128 单位四边形（0x46b900 顶点缓冲）+ 纹理矩阵（0x450）；本实现在 CPU 上完成
//            D3D 固定管线的变换，输出带 rhw 的屏幕顶点，交由共享后端做透视正确插值。
// 其余模式（条带/粒子/特效）待对应子系统接入时按同址扩展。
//
// 窗口缩放 DAT_004d9d34（0x45b280）默认 1.0；ins_313 的 scale mode 2/4 对其再 ×0.5。
#pragma once
#include "AnmManager.hpp"
#include "Types.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace th16 {

struct AnmQuad {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0, u2 = 0, v2 = 0, u3 = 0, v3 = 0;
    // D3D 视口深度（0..1）与 rhw（=1/w_clip）；屏幕空间模式恒为 z=0, rhw=1
    float z[4] = {0, 0, 0, 0};
    float rhw[4] = {1, 1, 1, 1};
    u32 color0 = 0, color1 = 0, color2 = 0, color3 = 0;
    u32 texture = 0;
    i32 blend = 0;
    i32 layer = 0;
    i32 script = 0;
    i32 file_slot = 0;
    u32 id = 0;
    bool nearest = false;
    bool depth_write = false;     // D3DRS_ZWRITEENABLE：0x467410 默认开（flags_lo 0x2000 关），屏幕模式关
    u8 address_u = 1, address_v = 1;  // D3DTADDRESS：1 WRAP 2 MIRROR 3 CLAMP（0x464f10）
};

// 0x43c780 建立的相机：视口中心正对 z=0 平面，距离 (h/2)/tan(fov/2)，up=(0,-1,0)，
// PerspectiveFovLH(fov, w/h, 1, 10000)。UI 相机 0x4c1560 由 0x43cb10 以 fov=π/6、全窗口视口初始化。
struct AnmCamera {
    float fov = 0.5235988f;        // +0x54（0x3f060a92）
    i32 viewport_x = 0, viewport_y = 0, viewport_w = 640, viewport_h = 480;  // +0xe0..+0xec
    float view[16] = {};           // +0x60
    float projection[16] = {};     // +0xa0
    void setup();                  // 0x43c780
    // 世界坐标 → D3D 屏幕坐标（像素）、视口深度、rhw
    void project(const float world[3], float& sx, float& sy, float& sz, float& rhw) const;
};

class AnmRenderer {
public:
    // 解析纹理句柄：file_slot + entry_index → GraphicsDevice 句柄；返回 0 = 跳过
    using TextureLookup = std::function<u32(int file_slot, int entry_index)>;

    float window_scale = 1.0f;   // DAT_004d9d34
    float draw_offset_x = 0;     // AnmManager +0xd0
    float draw_offset_y = 0;     // AnmManager +0xd4
    float field_offset_x = 0;    // DAT_004d9d50（layer group 0x40000）
    float field_offset_y = 0;    // DAT_004d9d54
    float screen_offset_x = 0;   // DAT_004d9d48（layer group 其它非零）
    float screen_offset_y = 0;   // DAT_004d9d4c
    AnmCamera camera;            // 当前 DAT_004c17b8 相机（标题/HUD 为 0x4c1560 UI 相机）
    TextureLookup lookup;
    std::string error;

    AnmRenderer() { camera.setup(); }

    // 0x468490：单 VM；成功写入 out 并返回 true；不可见 / 尚未实现的模式返回 false（非错误）
    bool emit(AnmManager& manager, AnmVm& vm, AnmQuad& out);
    // 0x46e750：遍历一层绘制链
    bool emit_layer(AnmManager& manager, int layer, std::vector<AnmQuad>& out);
    // 遍历 0..35 全部非空层（启动幕 / 调试用）
    bool emit_all(AnmManager& manager, std::vector<AnmQuad>& out);

private:
    // 0x406a70：按 scale mode 缩放、沿父链累加（父旋转 flag 0x800000）或加 layer group 偏移
    Vec3 transform_position(const AnmVm& vm, Vec3 p) const;
    // 0x406c40 + 0x406a70：把 VM 位置变换到屏幕空间
    Vec3 world_position(const AnmVm& vm) const;
    // 0x467410：mode 8 世界空间四边形经相机投影
    bool emit_projected(AnmManager& manager, AnmVm& vm, const AnmRuntimeSprite& sprite, AnmQuad& out);
    // 0x465c40：轴对齐四角（未加位置）
    void axis_corners(const AnmVm& vm, float out[4][2]) const;
    // 0x4660b0：带 Z 旋转的四角
    void rotated_corners(const AnmVm& vm, float out[4][2]) const;
    // 颜色：VM +0x520 的 BGRA 字节 → 0xAARRGGBB（D3DCOLOR / 共享后端约定）
    static u32 pack_color(const u8 bgra[4]) {
        return (u32(bgra[3]) << 24) | (u32(bgra[2]) << 16) | (u32(bgra[1]) << 8) | u32(bgra[0]);
    }
};

}  // namespace th16
