// th16 相机实现——地址见 Camera.hpp。矩阵公式按 D3DX9 参考实现逐项展开，保持单精度运算顺序。
#include "Camera.hpp"

#include <cmath>

namespace th16 {

namespace matrix {

void identity(float m[16]) {
    for (int k = 0; k < 16; ++k) m[k] = (k % 5 == 0) ? 1.0f : 0.0f;
}

void multiply(float out[16], const float a[16], const float b[16]) {
    float r[16];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] +
                           a[i * 4 + 2] * b[2 * 4 + j] + a[i * 4 + 3] * b[3 * 4 + j];
        }
    }
    for (int k = 0; k < 16; ++k) out[k] = r[k];
}

void rotation_x(float m[16], float angle) {
    identity(m);
    const float c = std::cos(angle), s = std::sin(angle);
    m[5] = c; m[6] = s; m[9] = -s; m[10] = c;
}

void rotation_y(float m[16], float angle) {
    identity(m);
    const float c = std::cos(angle), s = std::sin(angle);
    m[0] = c; m[2] = -s; m[8] = s; m[10] = c;
}

void rotation_z(float m[16], float angle) {
    identity(m);
    const float c = std::cos(angle), s = std::sin(angle);
    m[0] = c; m[1] = s; m[4] = -s; m[5] = c;
}

void translation(float m[16], float x, float y, float z) {
    identity(m);
    m[12] = x; m[13] = y; m[14] = z;
}

namespace {
void normalize(float v[3]) {
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len == 0.0f) { v[0] = v[1] = v[2] = 0.0f; return; }
    v[0] /= len; v[1] /= len; v[2] /= len;
}
}  // namespace

void look_at_lh(float m[16], const Vec3& eye, const Vec3& at, const Vec3& up) {
    // zaxis = normalize(at - eye)，xaxis = normalize(cross(up, zaxis))，yaxis = cross(zaxis, xaxis)
    float z[3] = {at.x - eye.x, at.y - eye.y, at.z - eye.z};
    normalize(z);
    float x[3] = {up.y * z[2] - up.z * z[1], up.z * z[0] - up.x * z[2], up.x * z[1] - up.y * z[0]};
    normalize(x);
    const float y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
    identity(m);
    m[0] = x[0]; m[1] = y[0]; m[2] = z[0];
    m[4] = x[1]; m[5] = y[1]; m[6] = z[1];
    m[8] = x[2]; m[9] = y[2]; m[10] = z[2];
    m[12] = -(x[0] * eye.x + x[1] * eye.y + x[2] * eye.z);
    m[13] = -(y[0] * eye.x + y[1] * eye.y + y[2] * eye.z);
    m[14] = -(z[0] * eye.x + z[1] * eye.y + z[2] * eye.z);
}

void perspective_fov_lh(float m[16], float fov, float aspect, float zn, float zf) {
    for (int k = 0; k < 16; ++k) m[k] = 0.0f;
    const float half_tan = std::tan(fov / 2.0f);
    m[0] = 1.0f / (aspect * half_tan);
    m[5] = 1.0f / half_tan;
    m[10] = zf / (zf - zn);
    m[11] = 1.0f;
    m[14] = (zf * zn) / (zn - zf);
}

void transform_point(const float v[3], const float m[16], float out[4]) {
    for (int j = 0; j < 4; ++j) out[j] = v[0] * m[0 * 4 + j] + v[1] * m[1 * 4 + j] + v[2] * m[2 * 4 + j] + m[3 * 4 + j];
}

}  // namespace matrix

void Camera::reset_to_viewport_center() {
    // 0x43c780：视口中心 (x + w/2, y + h/2)，相机沿 -Z 退到 (h>>1)/tan(fov/2) 处看向 z=0 平面，up=(0,-1,0)
    const float cx = float(viewport.x) + float(viewport.width) * 0.5f;
    const float cy = float(viewport.height) * 0.5f + float(viewport.y);
    const float distance = (float(u32(viewport.height) >> 1) + 0.0f) / std::tan(fov * 0.5f);
    matrix::look_at_lh(view, Vec3{cx, cy, distance}, Vec3{cx, cy, 0.0f}, Vec3{0.0f, -1.0f, 0.0f});
    matrix::perspective_fov_lh(projection, fov, float(viewport.width) / float(viewport.height), 1.0f, 10000.0f);
}

void Camera::look_along_facing() {
    // 0x43c940：eye = pos + pos_offset，at = eye + facing_norm；近 30、远 8000
    matrix::look_at_lh(view, eye(), at(), up);
    matrix::perspective_fov_lh(projection, fov, float(viewport.width) / float(viewport.height), 30.0f, 8000.0f);
    // right = normalize(cross(up, facing_norm))（分量写入顺序与原版一致：先 y、x，再 z）
    float r[3] = {up.y * facing_norm.z - up.z * facing_norm.y,
                  up.z * facing_norm.x - up.x * facing_norm.z,
                  up.x * facing_norm.y - up.y * facing_norm.x};
    matrix::normalize(r);
    right = {r[0], r[1], r[2]};
}

void Camera::project(const float world[3], float& sx, float& sy, float& sz, float& rhw) const {
    float eye_space[4], clip[4];
    matrix::transform_point(world, view, eye_space);
    for (int j = 0; j < 4; ++j) {
        clip[j] = eye_space[0] * projection[0 * 4 + j] + eye_space[1] * projection[1 * 4 + j] +
                  eye_space[2] * projection[2 * 4 + j] + eye_space[3] * projection[3 * 4 + j];
    }
    const float w = clip[3] != 0.0f ? clip[3] : 1e-6f;
    const float inv = 1.0f / w;
    sx = float(viewport.x) + (1.0f + clip[0] * inv) * float(viewport.width) * 0.5f;
    sy = float(viewport.y) + (1.0f - clip[1] * inv) * float(viewport.height) * 0.5f;
    sz = clip[2] * inv;
    rhw = inv;
}

}  // namespace th16
