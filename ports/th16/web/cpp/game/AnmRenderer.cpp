// th16 ANM 绘制实现——地址见 AnmRenderer.hpp。
#include "AnmRenderer.hpp"

#include <algorithm>

namespace th16 {

namespace {

// DAT_004a3048 / DAT_004a3078：锚点 × 精灵宽高 → 四角本地坐标（TL, TR, BL, BR）
constexpr float kAnchorX[4][4] = {
    {-0.5f, 0.5f, -0.5f, 0.5f},
    {0.0f, 1.0f, 0.0f, 1.0f},
    {-1.0f, 0.0f, -1.0f, 0.0f},
    {-0.5f, -0.5f, 0.5f, 0.5f},
};
constexpr float kAnchorY[4][4] = {
    {-0.5f, -0.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 1.0f, 1.0f},
    {-1.0f, -1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
};

float scale_factor(const AnmVm& vm, float window_scale) {
    // 0x465c40 / 0x406a70：flags_hi 的 scale mode（ins_313）
    const u32 mode = (vm.flags_hi >> 20) & 7u;
    if (mode == 1 || mode == 3) return window_scale;
    if (mode == 2 || mode == 4) return window_scale * 0.5f;
    return 1.0f;
}

}  // namespace

Vec3 AnmRenderer::world_position(const AnmVm& vm) const {
    // 0x406c40：pos + entity_offset + pos2
    Vec3 p{
        vm.pos.x + vm.entity_offset.x + vm.pos2.x,
        vm.pos.y + vm.entity_offset.y + vm.pos2.y,
        vm.pos.z + vm.entity_offset.z + vm.pos2.z,
    };
    // 0x406a70：scale mode → 乘窗口缩放；再沿 root 链累加（未分离时）
    const float s = scale_factor(vm, window_scale);
    if (s != 1.0f) { p.x *= s; p.y *= s; p.z *= s; }
    if (vm.root != nullptr && (vm.flags_hi & AnmVm::kHiDetached) == 0) {
        // 父旋转（flags_hi bit 23）暂未在启动幕出现；先累加父世界坐标
        const Vec3 parent = world_position(*vm.root);
        p.x += parent.x;
        p.y += parent.y;
        p.z += parent.z;
    } else {
        const u32 group = vm.flags_hi & 0xc0000u;
        if (group != 0) {
            if (group == 0x40000u) { p.x += field_offset_x; p.y += field_offset_y; }
            else { p.x += screen_offset_x; p.y += screen_offset_y; }
        }
    }
    return p;
}

void AnmRenderer::axis_corners(const AnmVm& vm, float out[4][2]) const {
    // 0x465c40 顺序：锚点×精灵尺寸 → 减 anchor_offset → ×窗口缩放（scale mode）→ × scale/scale2
    const int ax = int((vm.flags_lo >> 21) & 3u);
    const int ay = int((vm.flags_lo >> 23) & 3u);
    for (int i = 0; i < 4; ++i) {
        out[i][0] = kAnchorX[ax][i] * vm.sprite_size.x - vm.anchor_offset.x;
        out[i][1] = kAnchorY[ay][i] * vm.sprite_size.y - vm.anchor_offset.y;
    }
    const float window = scale_factor(vm, window_scale);
    if (window != 1.0f) for (int i = 0; i < 4; ++i) { out[i][0] *= window; out[i][1] *= window; }
    float sx = vm.scale.x * vm.scale2.x;
    float sy = vm.scale.y * vm.scale2.y;
    if (vm.parent != nullptr && (vm.flags_hi & AnmVm::kHiDetached) == 0) {
        sx *= vm.parent->scale.x * vm.parent->scale2.x;
        sy *= vm.parent->scale.y * vm.parent->scale2.y;
    }
    for (int i = 0; i < 4; ++i) { out[i][0] *= sx; out[i][1] *= sy; }
}

void AnmRenderer::rotated_corners(const AnmVm& vm, float out[4][2]) const {
    axis_corners(vm, out);
    // 0x464dd0 结果的 Z 分量；启动幕 rotation 为 0，走 cos=1/sin=0 快路径亦可
    const float angle = vm.world_rotation.z != 0.0f ? vm.world_rotation.z : vm.rotation.z;
    if (angle == 0.0f) return;
    const float c = std::cos(angle), s = std::sin(angle);
    for (int i = 0; i < 4; ++i) {
        const float x = out[i][0], y = out[i][1];
        out[i][0] = x * c - y * s;
        out[i][1] = x * s + y * c;
    }
}

bool AnmRenderer::emit(AnmManager& manager, AnmVm& vm, AnmQuad& out) {
    // 0x468490 入口条件：可见 + flag2 + 非删除中
    if ((vm.flags_lo & 1u) == 0 || (vm.flags_lo & 2u) == 0) return false;
    if ((vm.flags_hi & 0x60u) != 0) return false;
    if (vm.sprite_index < 0) return false;
    const AnmFile* file = manager.file(vm.file_slot);
    // 越界精灵：原版靠 sprite_mapper（0x491b1c）重映射；映射表未接前静默跳过，不视为致命错误。
    if (file == nullptr || u32(vm.sprite_index) >= file->sprites.size()) return false;
    const AnmRuntimeSprite& sprite = file->sprites[u32(vm.sprite_index)];
    if (!lookup) { error = "ANM texture lookup not set"; return false; }
    const u32 texture = lookup(sprite.file_slot, sprite.entry_index);
    if (texture == 0) return false;

    // alpha 全 0 时原版亦跳过（mode 0..3 检查 color[3] / color2[3]）
    if (vm.color[3] == 0 && vm.color2[3] == 0) return false;

    const int mode = vm.render_mode();
    if (mode > 3) {
        // 尚未实现的模式：静默跳过（非致命）；启动幕只用 mode 0
        return false;
    }

    float corners[4][2];
    if (mode == 0 || mode == 2) axis_corners(vm, corners);
    else rotated_corners(vm, corners);

    const Vec3 origin = world_position(vm);
    for (int i = 0; i < 4; ++i) {
        corners[i][0] += origin.x + draw_offset_x;
        corners[i][1] += origin.y + draw_offset_y;
    }
    // mode 0：像素对齐（ROUND - 0.5）；与 0x465280 一致
    if (mode == 0) {
        auto snap = [](float v) { return std::nearbyintf(v) - 0.5f; };
        corners[0][0] = snap(corners[0][0]);
        corners[1][0] = snap(corners[1][0]);
        corners[0][1] = snap(corners[0][1]);
        corners[2][1] = snap(corners[2][1]);
        corners[1][1] = corners[0][1];
        corners[2][0] = corners[0][0];
        corners[3][0] = corners[1][0];
        corners[3][1] = corners[2][1];
    }

    // UV：基 UV + 滚动；uv_scale 缩放宽/高分量（0x465280）
    const float u00 = vm.uv_scroll.x + vm.uv[0].x;
    const float v00 = vm.uv_scroll.y + vm.uv[0].y;
    const float u10 = (vm.uv[1].x - vm.uv[0].x) * vm.uv_scale.x + vm.uv_scroll.x + vm.uv[0].x;
    const float v10 = vm.uv_scroll.y + vm.uv[1].y;
    const float u01 = vm.uv_scroll.x + vm.uv[2].x;
    const float v01 = (vm.uv[2].y - vm.uv[0].y) * vm.uv_scale.y + vm.uv_scroll.y + vm.uv[0].y;
    const float u11 = (vm.uv[3].x - vm.uv[2].x) * vm.uv_scale.x + vm.uv_scroll.x + vm.uv[2].x;
    const float v11 = (vm.uv[3].y - vm.uv[1].y) * vm.uv_scale.y + vm.uv_scroll.y + vm.uv[1].y;

    const u32 color = pack_color(vm.color);
    out = {};
    out.x0 = corners[0][0]; out.y0 = corners[0][1];
    out.x1 = corners[1][0]; out.y1 = corners[1][1];
    out.x2 = corners[2][0]; out.y2 = corners[2][1];
    out.x3 = corners[3][0]; out.y3 = corners[3][1];
    out.u0 = u00; out.v0 = v00;
    out.u1 = u10; out.v1 = v10;
    out.u2 = u01; out.v2 = v01;
    out.u3 = u11; out.v3 = v11;
    out.color0 = out.color1 = out.color2 = out.color3 = color;
    out.texture = texture;
    out.blend = vm.blend_mode();
    out.layer = vm.layer;
    out.script = vm.script_index;
    out.id = vm.id;
    out.nearest = (vm.flags_hi & 0x800u) != 0;  // ins_311 → 最近邻采样
    return true;
}

bool AnmRenderer::emit_layer(AnmManager& manager, int layer, std::vector<AnmQuad>& out) {
    if (layer < 0 || layer >= AnmManager::kLayerCount) return true;
    for (AnmVm* vm = manager.layer_chain(layer); vm != nullptr; vm = vm->layer_next) {
        AnmQuad quad;
        if (emit(manager, *vm, quad)) out.push_back(quad);
        else if (!error.empty()) return false;
    }
    return true;
}

bool AnmRenderer::emit_all(AnmManager& manager, std::vector<AnmQuad>& out) {
    for (int layer = 0; layer < AnmManager::kLayerCount; ++layer) {
        if (!emit_layer(manager, layer, out)) return false;
    }
    return true;
}

}  // namespace th16
