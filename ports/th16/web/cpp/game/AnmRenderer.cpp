// th16 ANM 绘制实现——地址见 AnmRenderer.hpp。
#include "AnmRenderer.hpp"

#include <algorithm>
#include <cstring>

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

// 0x464f10：flags → D3DTADDRESS（0 WRAP=1，1 CLAMP=3，2 MIRROR=2；3 保持原状，这里取 WRAP）
u8 address_mode(u32 bits) {
    switch (bits & 3u) {
    case 1: return 3;
    case 2: return 2;
    default: return 1;
    }
}

float scale_factor(const AnmVm& vm, float window_scale) {
    // 0x465c40 / 0x406a70：flags_hi 的 scale mode（ins_313）
    const u32 mode = (vm.flags_hi >> 20) & 7u;
    if (mode == 1 || mode == 3) return window_scale;
    if (mode == 2 || mode == 4) return window_scale * 0.5f;
    return 1.0f;
}

// ---- D3DX 行向量矩阵（v' = v · M）----
using Mat4 = float[16];

void mat_identity(Mat4 m) { for (int k = 0; k < 16; ++k) m[k] = (k % 5 == 0) ? 1.0f : 0.0f; }

// D3DXMatrixMultiply(out, a, b) = a · b
void mat_multiply(Mat4 out, const Mat4 a, const Mat4 b) {
    Mat4 r;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] +
                           a[i * 4 + 2] * b[2 * 4 + j] + a[i * 4 + 3] * b[3 * 4 + j];
        }
    }
    for (int k = 0; k < 16; ++k) out[k] = r[k];
}

void mat_rotation_x(Mat4 m, float a) {
    mat_identity(m);
    const float c = std::cos(a), s = std::sin(a);
    m[5] = c; m[6] = s; m[9] = -s; m[10] = c;
}
void mat_rotation_y(Mat4 m, float a) {
    mat_identity(m);
    const float c = std::cos(a), s = std::sin(a);
    m[0] = c; m[2] = -s; m[8] = s; m[10] = c;
}
void mat_rotation_z(Mat4 m, float a) {
    mat_identity(m);
    const float c = std::cos(a), s = std::sin(a);
    m[0] = c; m[1] = s; m[4] = -s; m[5] = c;
}

void vec_transform(const float v[3], const Mat4 m, float out[4]) {
    for (int j = 0; j < 4; ++j) out[j] = v[0] * m[0 * 4 + j] + v[1] * m[1 * 4 + j] + v[2] * m[2 * 4 + j] + m[3 * 4 + j];
}

}  // namespace

void AnmCamera::setup() {
    // 0x43c780：eye=(vx+vw/2, vy+vh/2, (vh>>1)/tan(fov/2))，at=(同 xy, 0)，up=(0,-1,0)
    const float cx = float(viewport_x) + float(viewport_w) * 0.5f;
    const float cy = float(viewport_h) * 0.5f + float(viewport_y);
    const float d = float(u32(viewport_h) >> 1) / std::tan(fov * 0.5f);
    // D3DXMatrixLookAtLH：zaxis=(0,0,-1) xaxis=(1,0,0) yaxis=(0,-1,0)
    mat_identity(view);
    view[5] = -1.0f;
    view[10] = -1.0f;
    view[12] = -cx;
    view[13] = cy;
    view[14] = d;
    // D3DXMatrixPerspectiveFovLH(fov, w/h, 1, 10000)
    const float y_scale = 1.0f / std::tan(fov * 0.5f);
    const float aspect = float(viewport_w) / float(viewport_h);
    const float zn = 1.0f, zf = 10000.0f;
    for (float& k : projection) k = 0.0f;
    projection[0] = y_scale / aspect;
    projection[5] = y_scale;
    projection[10] = zf / (zf - zn);
    projection[11] = 1.0f;
    projection[14] = -zn * zf / (zf - zn);
}

void AnmCamera::look_at(const Vec3& eye, const Vec3& at, const Vec3& up, float zn, float zf) {
    // D3DXMatrixLookAtLH：zaxis = normalize(at - eye)，xaxis = normalize(cross(up, zaxis))，yaxis = cross(zaxis, xaxis)
    auto normalize = [](float v[3]) {
        const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (len > 0.0f) { v[0] /= len; v[1] /= len; v[2] /= len; }
    };
    float z[3] = {at.x - eye.x, at.y - eye.y, at.z - eye.z};
    normalize(z);
    float x[3] = {up.y * z[2] - up.z * z[1], up.z * z[0] - up.x * z[2], up.x * z[1] - up.y * z[0]};
    normalize(x);
    const float y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
    mat_identity(view);
    view[0] = x[0]; view[1] = y[0]; view[2] = z[0];
    view[4] = x[1]; view[5] = y[1]; view[6] = z[1];
    view[8] = x[2]; view[9] = y[2]; view[10] = z[2];
    view[12] = -(x[0] * eye.x + x[1] * eye.y + x[2] * eye.z);
    view[13] = -(y[0] * eye.x + y[1] * eye.y + y[2] * eye.z);
    view[14] = -(z[0] * eye.x + z[1] * eye.y + z[2] * eye.z);
    const float y_scale = 1.0f / std::tan(fov * 0.5f);
    const float aspect = float(viewport_w) / float(viewport_h);
    for (float& k : projection) k = 0.0f;
    projection[0] = y_scale / aspect;
    projection[5] = y_scale;
    projection[10] = zf / (zf - zn);
    projection[11] = 1.0f;
    projection[14] = -zn * zf / (zf - zn);
}

void AnmCamera::project(const float world[3], float& sx, float& sy, float& sz, float& rhw) const {
    float eye[4], clip[4];
    vec_transform(world, view, eye);
    for (int j = 0; j < 4; ++j) {
        clip[j] = eye[0] * projection[0 * 4 + j] + eye[1] * projection[1 * 4 + j] +
                  eye[2] * projection[2 * 4 + j] + eye[3] * projection[3 * 4 + j];
    }
    const float w = clip[3] != 0.0f ? clip[3] : 1e-6f;
    const float inv = 1.0f / w;
    // D3D 视口变换（MinZ 0 / MaxZ 1）
    sx = float(viewport_x) + (1.0f + clip[0] * inv) * float(viewport_w) * 0.5f;
    sy = float(viewport_y) + (1.0f - clip[1] * inv) * float(viewport_h) * 0.5f;
    sz = clip[2] * inv;
    rhw = inv;
}

Vec3 AnmRenderer::transform_position(const AnmVm& vm, Vec3 p) const {
    // 0x406a70：scale mode → 乘窗口缩放；再沿 root 链累加（未分离时）或加 layer group 偏移
    const float s = scale_factor(vm, window_scale);
    if (s != 1.0f) { p.x *= s; p.y *= s; p.z *= s; }
    if (vm.root != nullptr && (vm.flags_hi & AnmVm::kHiDetached) == 0) {
        if ((vm.flags_hi & AnmVm::kHi314) != 0) {
            // 随父 Z 旋转（0x405510/0x4054f0 = sin/cos(root.rotation.z)）
            const float sn = std::sin(vm.root->rotation.z), cs = std::cos(vm.root->rotation.z);
            const float x = p.x;
            p.x = x * cs - p.y * sn;
            p.y = p.y * cs + x * sn;
        }
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

Vec3 AnmRenderer::world_position(const AnmVm& vm) const {
    // 0x406c40：pos + entity_offset + pos2 → 0x406a70
    return transform_position(vm, Vec3{
        vm.pos.x + vm.entity_offset.x + vm.pos2.x,
        vm.pos.y + vm.entity_offset.y + vm.pos2.y,
        vm.pos.z + vm.entity_offset.z + vm.pos2.z,
    });
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
    // 0x4660b0：0x464dd0 结果的 Z 分量
    const float angle = anm_world_rotation(const_cast<AnmVm&>(vm)).z;
    if (angle == 0.0f) return;
    const float c = std::cos(angle), s = std::sin(angle);
    for (int i = 0; i < 4; ++i) {
        const float x = out[i][0], y = out[i][1];
        out[i][0] = x * c - y * s;
        out[i][1] = x * s + y * c;
    }
}

bool AnmRenderer::vm_corners(const AnmVm& vm, float out[4][3]) const {
    // 0x468c00 → 0x465c40 / 0x4660b0：四角偏移 + 0x406c40 位置；Z = entity_offset.z + pos.z + pos2.z
    float corners[4][2];
    switch (vm.render_mode()) {
    case 0: case 2: case 3: axis_corners(vm, corners); break;
    case 1: rotated_corners(vm, corners); break;
    default: return false;
    }
    const Vec3 origin = world_position(vm);
    const float z = vm.entity_offset.z + vm.pos.z + vm.pos2.z;
    for (int i = 0; i < 4; ++i) {
        out[i][0] = corners[i][0] + origin.x;
        out[i][1] = corners[i][1] + origin.y;
        out[i][2] = z;
    }
    return true;
}

bool AnmRenderer::emit(AnmManager& manager, AnmVm& vm, AnmQuad& out) {
    // 0x468490 入口条件：可见 + flag2 + 非删除中
    if ((vm.flags_lo & 1u) == 0 || (vm.flags_lo & 2u) == 0) return false;
    if ((vm.flags_hi & 0x60u) != 0) return false;
    if (vm.sprite_index < 0) return false;
    // 精灵记录取自 UV 来源文件（ins_300(-1) 时为 ascii.anm，见 AnmVm::sprite_source_slot）
    const AnmFile* file = manager.file(vm.sprite_source_slot >= 0 ? vm.sprite_source_slot : vm.file_slot);
    if (file == nullptr || u32(vm.sprite_index) >= file->sprites.size()) return false;
    const AnmRuntimeSprite& sprite = file->sprites[u32(vm.sprite_index)];
    if (!lookup) { error = "ANM texture lookup not set"; return false; }
    const u32 texture = lookup(sprite.file_slot, sprite.entry_index);
    if (texture == 0) return false;

    const int mode = vm.render_mode();
    out = {};
    out.texture = texture;
    out.file_slot = vm.file_slot;
    if (mode == 8) return emit_projected(manager, vm, sprite, out);

    // alpha 全 0 时原版亦跳过（mode 0..3 检查 color[3] / color2[3]）
    if (vm.color[3] == 0 && vm.color2[3] == 0) return false;

    if (mode > 3) {
        // 尚未实现的模式：静默跳过（非致命）
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
    out.depth_write = false;                    // 0x468490 先 0x43c570 关 Z 写
    out.address_u = address_mode(vm.flags_hi & 3u);
    out.address_v = address_mode(vm.flags_lo >> 30);
    return true;
}

bool AnmRenderer::emit_projected(AnmManager&, AnmVm& vm, const AnmRuntimeSprite&, AnmQuad& out) {
    // 0x467410 入口：可见 + flag2 + color alpha 非 0（只看 0x523）
    if (vm.color[3] == 0) return false;

    // 世界矩阵 0x410：未冻结（flags_lo 0x10000）时由 0x3d0 重建 → 缩放 → 旋转
    if ((vm.flags_lo & 0x10000u) == 0) {
        float* m = vm.texture_matrix_copy;
        std::memcpy(m, vm.texture_matrix, sizeof vm.texture_matrix);
        m[0] *= vm.scale2.x * vm.scale.x;
        m[5] *= vm.scale2.y * vm.scale.y;
        vm.flags_lo &= ~AnmVm::kScaleDirty;
        const u32 scale_mode = vm.flags_hi & AnmVm::kHiScaleModeMask;
        if (scale_mode == 0x100000u) { m[0] *= window_scale; m[5] *= window_scale; }
        else if (scale_mode == 0x200000u) { m[0] *= window_scale * 0.5f; m[5] *= window_scale * 0.5f; }

        const Vec3 r = anm_world_rotation(vm);
        Mat4 rot;
        auto apply = [&](char axis, float angle) {
            if (angle == 0.0f) return;
            if (axis == 'x') mat_rotation_x(rot, angle);
            else if (axis == 'y') mat_rotation_y(rot, angle);
            else mat_rotation_z(rot, angle);
            mat_multiply(m, m, rot);
        };
        // flags_hi bits 2..4（ins_437）：欧拉旋转顺序
        switch ((vm.flags_hi >> 2) & 7u) {
        case 0: apply('x', r.x); apply('y', r.y); apply('z', r.z); break;
        case 1: apply('x', r.x); apply('z', r.z); apply('y', r.y); break;
        case 2: apply('y', r.y); apply('x', r.x); apply('z', r.z); break;
        case 3: apply('y', r.y); apply('z', r.z); apply('x', r.x); break;
        case 4: apply('z', r.z); apply('x', r.x); apply('y', r.y); break;
        case 5: apply('z', r.z); apply('y', r.y); apply('x', r.x); break;
        default: break;
        }
        vm.flags_lo &= ~AnmVm::kRotationDirty;
    }

    // 平移行：(pos2 + pos + entity_offset) − anchor_offset × scale × scale2 → 0x406a70；z 取未经变换的和
    Vec3 t = transform_position(vm, Vec3{
        (vm.entity_offset.x + vm.pos.x + vm.pos2.x) - vm.anchor_offset.x * vm.scale.x * vm.scale2.x,
        (vm.entity_offset.y + vm.pos.y + vm.pos2.y) - vm.anchor_offset.y * vm.scale.y * vm.scale2.y,
        vm.texture_matrix_copy[14],
    });
    t.z = vm.entity_offset.z + vm.pos.z + vm.pos2.z;
    Mat4 world;
    std::memcpy(world, vm.texture_matrix_copy, sizeof world);
    world[12] = t.x;
    world[13] = t.y;
    world[14] = t.z;
    world[15] = vm.texture_matrix_copy[15];

    // 0x46b900 顶点缓冲：±128 单位四边形按锚点（anchorY*3+anchorX）平移；顺序 TL TR BL BR，UV (0,0)(1,0)(0,1)(1,1)
    const u32 ax = (vm.flags_lo >> 21) & 3u;
    const u32 ay = (vm.flags_lo >> 23) & 3u;
    const float ox = ax == 1 ? 128.0f : ax == 2 ? -128.0f : 0.0f;
    const float oy = ay == 1 ? 128.0f : ay == 2 ? -128.0f : 0.0f;
    const float local[4][3] = {
        {-128.0f + ox, -128.0f + oy, 0.0f},
        {128.0f + ox, -128.0f + oy, 0.0f},
        {-128.0f + ox, 128.0f + oy, 0.0f},
        {128.0f + ox, 128.0f + oy, 0.0f},
    };
    float* xs[4] = {&out.x0, &out.x1, &out.x2, &out.x3};
    float* ys[4] = {&out.y0, &out.y1, &out.y2, &out.y3};
    for (int i = 0; i < 4; ++i) {
        float w[4];
        vec_transform(local[i], world, w);
        active_camera_->project(w, *xs[i], *ys[i], out.z[i], out.rhw[i]);
    }

    // 纹理矩阵 D3DTS_TEXTURE0：u' = u × uv_scale.x × sprite_matrix[0] + (u0 + scroll.x)，v 同理
    const float su = vm.uv_scale.x * vm.sprite_matrix[0];
    const float sv = vm.uv_scale.y * vm.sprite_matrix[5];
    const float u0 = vm.uv[0].x + vm.uv_scroll.x;
    const float v0 = vm.uv[0].y + vm.uv_scroll.y;
    out.u0 = u0;      out.v0 = v0;
    out.u1 = u0 + su; out.v1 = v0;
    out.u2 = u0;      out.v2 = v0 + sv;
    out.u3 = u0 + su; out.v3 = v0 + sv;

    // D3DRS_TEXTUREFACTOR：渐变位（0x60000）非 0 时取 color2，否则 color
    const u32 color = pack_color((vm.flags_lo & AnmVm::kColorModeMask) == 0 ? vm.color : vm.color2);
    out.color0 = out.color1 = out.color2 = out.color3 = color;
    out.blend = vm.blend_mode();
    out.layer = vm.layer;
    out.script = vm.script_index;
    out.id = vm.id;
    out.nearest = (vm.flags_hi & 0x800u) != 0;
    out.depth_write = (vm.flags_lo & AnmVm::kZWriteDisable) == 0;  // 0x43c530 / 0x43c570
    out.address_u = address_mode(vm.flags_hi & 3u);
    out.address_v = address_mode(vm.flags_lo >> 30);
    return true;
}

bool AnmRenderer::emit_stage(AnmManager& manager, StageManager& stage, StagePass& out) {
    out.active = false;
    out.quads.clear();
    if ((stage.flags & 8) != 0) return true;
    const bool ending = (stage.flags & 4) != 0;
    if (ending && stage.end_timer.current >= 0x3c) return true;

    // 0x409f90 → 0x43c940：eye = pos + pos_offset，at = eye + facing_norm；right = normalize(cross(up, facing_norm))
    StageCamera& sc = stage.camera;
    AnmCamera cam;
    cam.fov = sc.fov;
    cam.viewport_x = sc.viewport_x; cam.viewport_y = sc.viewport_y;
    cam.viewport_w = sc.viewport_w; cam.viewport_h = sc.viewport_h;
    cam.look_at(stage.eye(), stage.at(), sc.up, StageManager::kNearZ, StageManager::kFarZ);
    {
        const Vec3& u = sc.up; const Vec3& f = sc.facing_norm;
        float r[3] = {u.y * f.z - u.z * f.y, u.z * f.x - u.x * f.z, u.x * f.y - u.y * f.x};
        const float len = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
        if (len > 0.0f) { r[0] /= len; r[1] /= len; r[2] /= len; }
        sc.right = {r[0], r[1], r[2]};
    }
    out.active = true;
    out.fog_color = sc.fog.color;
    out.fog_near = sc.fog.near_;
    out.fog_far = sc.fog.far_;
    out.clear_color = (ending && stage.end_timer.current <= 0x21) ? 0u : sc.fog.color;
    if ((stage.flags & 1) == 0) return true;

    const AnmCamera* saved = active_camera_;
    active_camera_ = &cam;
    bool ok = true;
    for (int layer = 0; layer < 8 && ok; ++layer) {
        stage.draw_layer(layer, [&](AnmVm& vm, bool is_sprite) {
            if (!ok) return;
            AnmQuad quad;
            if (!emit(manager, vm, quad)) { if (!error.empty()) ok = false; return; }
            const int mode = vm.render_mode();
            quad.depth_test = !is_sprite && (vm.flags_lo & AnmVm::kZWriteDisable) == 0;
            if (is_sprite) quad.depth_write = false;
            quad.fog = !is_sprite && (mode == 8 || mode == 0x18);
            if (quad.fog) {
                for (int v = 0; v < 4; ++v) {
                    // D3DRS_FOGVERTEXMODE = LINEAR：z_eye = w_clip = 1/rhw
                    const float z = quad.rhw[v] != 0.0f ? 1.0f / quad.rhw[v] : 0.0f;
                    float f = (sc.fog.far_ - z) / (sc.fog.far_ - sc.fog.near_);
                    if (f < 0.0f) f = 0.0f;
                    if (f > 1.0f) f = 1.0f;
                    quad.fog_factor[v] = u8(f * 255.0f + 0.5f);
                }
            }
            quad.x0 += StageManager::kFieldShiftX; quad.x1 += StageManager::kFieldShiftX;
            quad.x2 += StageManager::kFieldShiftX; quad.x3 += StageManager::kFieldShiftX;
            quad.layer = layer;
            out.quads.push_back(quad);
        });
    }
    active_camera_ = saved;
    return ok;
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
