#include "SceneMatrices.hpp"

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

}  // namespace th13
