// th16 ANM 绘制——原版 0x468490（按 layer 链逐 VM 绘制）的屏幕四边形路径。
//
// 本文件覆盖启动幕 / 标题 / HUD 所需的 render mode 0..3（屏幕空间四边形）：
//   0x465c40 轴对齐几何（mode 0/2）  0x4660b0 带旋转（mode 1/3）
//   0x465280 像素对齐、UV 滚动、颜色打包、提交 D3D DrawPrimitive
// 更高模式（投影/世界/条带/特效）待对应子系统接入时按同址扩展。
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
    u32 color0 = 0, color1 = 0, color2 = 0, color3 = 0;
    u32 texture = 0;
    i32 blend = 0;
    i32 layer = 0;
    i32 script = 0;
    u32 id = 0;
    bool nearest = false;
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
    TextureLookup lookup;
    std::string error;

    // 0x468490：单 VM；成功写入 out 并返回 true；不可见 / 尚未实现的模式返回 false（非错误）
    bool emit(AnmManager& manager, AnmVm& vm, AnmQuad& out);
    // 0x46e750：遍历一层绘制链
    bool emit_layer(AnmManager& manager, int layer, std::vector<AnmQuad>& out);
    // 遍历 0..35 全部非空层（启动幕 / 调试用）
    bool emit_all(AnmManager& manager, std::vector<AnmQuad>& out);

private:
    // 0x406c40 + 0x406a70：把 VM 位置变换到屏幕空间
    Vec3 world_position(const AnmVm& vm) const;
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
