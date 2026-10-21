#include "SceneMatrices.hpp"
#include "AnmTransforms.hpp"
#include "StageScript.hpp"

#include <cmath>

namespace th13 {

Vec3 normalize_scene_vector(Vec3 v) noexcept {
    const float squared =
        static_cast<float>(static_cast<float>(v.x * v.x + v.y * v.y) + v.z * v.z);
    if (!(squared >= 0x1p-46f)) return {};
    const float inverse = static_cast<float>(1.f / static_cast<float>(std::sqrt(double(squared))));
    const float refined = static_cast<float>(
        static_cast<float>(.5f * inverse) *
        static_cast<float>(3.f - static_cast<float>(squared * inverse * inverse)));
    return {static_cast<float>(v.x * refined), static_cast<float>(v.y * refined),
            static_cast<float>(v.z * refined)};
}

namespace {
Vec3 view_cross(Vec3 a, Vec3 b) {
    return {static_cast<float>(double(a.y) * b.z - double(a.z) * b.y),
            static_cast<float>(double(a.z) * b.x - double(a.x) * b.z),
            static_cast<float>(double(a.x) * b.y - double(a.y) * b.x)};
}
}  // namespace

Matrix4 scene_look_at(Vec3 eye, Vec3 target, Vec3 up) noexcept {
    const Vec3 z = normalize_scene_vector({target.x - eye.x, target.y - eye.y, target.z - eye.z});
    const Vec3 x = normalize_scene_vector(view_cross(up, z));
    const Vec3 y = view_cross(z, x);
    const Vec3 axes[] = {x, y, z};
    Matrix4 out;
    for (u32 c = 0; c < 3; ++c) {
        const auto a = axes[c];
        out.m[c] = a.x;
        out.m[4 + c] = a.y;
        out.m[8 + c] = a.z;
        out.m[12 + c] =
            -static_cast<float>((double(a.x) * eye.x + double(a.y) * eye.y) + double(a.z) * eye.z);
    }
    out.m[15] = 1;
    return out;
}

Matrix4 scene_perspective(float fov, float aspect, float near_plane, float far_plane) noexcept {
    const float half = static_cast<float>(fov * .5f);
    const float c = static_cast<float>(std::cos(double(half)));
    const float s = static_cast<float>(std::sin(double(half)));
    const float cot = static_cast<float>(c / s);
    Matrix4 out;
    out.m[0] = static_cast<float>(cot / aspect);
    out.m[5] = cot;
    out.m[10] = static_cast<float>(far_plane / static_cast<float>(far_plane - near_plane));
    out.m[11] = 1;
    out.m[14] = -out.m[10] * near_plane;
    return out;
}

namespace {
Vec3 add(Vec3 a, Vec3 b) { return {float(a.x + b.x), float(a.y + b.y), float(a.z + b.z)}; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {float(float(a.y * b.z) - float(a.z * b.y)), float(float(a.z * b.x) - float(a.x * b.z)),
            float(float(a.x * b.y) - float(a.y * b.x))};
}
}  // namespace

AnmCamera stage_camera(const StageCamera& s, const GraphicsViewport& viewport) noexcept {
    AnmCamera out;
    out.eye = add(s.position, s.eye_offset);
    out.view = scene_look_at(out.eye, add(out.eye, s.direction), s.up);
    // 后端世界路径带 NDC y 翻转（Shaders.hpp gl_Position.y 取负）：STD 的
    // +y（上方）需映到视口上缘，故取景矩阵的视线 y 基（列 1）镜像一次
    // （golden f1242：鸟居（相机上方）位于画面上部——方向实证）。
    for (u32 index : {1u, 5u, 9u, 13u}) out.view.m[index] = float(-out.view.m[index]);
    out.projection =
        scene_perspective(s.fov, float(float(viewport.width) / float(viewport.height)), 30, 8000);
    out.reference = normalize_scene_vector(cross(s.direction, s.up));
    out.fog_near = s.fog.near_distance;
    out.fog_far = s.fog.far_distance;
    out.fog_rgb = {s.fog.channels[2], s.fog.channels[1], s.fog.channels[0]};
    out.fog_color = s.fog.color;
    return out;
}

}  // namespace th13
