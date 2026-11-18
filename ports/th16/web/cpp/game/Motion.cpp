#include "Motion.hpp"

#include <cmath>

namespace th16 {

float wrap_angle(float a) noexcept {
    int n = 0;
    while (a > 3.1415927f) { a -= 6.2831855f; if (++n > 0x21) break; }
    while (a < -3.1415927f) { a += 6.2831855f; if (++n > 0x21) break; }
    return a;
}

// 必须经 double 再舍入：sinf/cosf 会偶发与原版 CRT 差 1 ulp
float sin_f(float a) noexcept { return float(std::sin(double(a))); }
float cos_f(float a) noexcept { return float(std::cos(double(a))); }
float floor_f(float a) noexcept { return float(std::floor(double(a))); }

// 不能先把 cos 舍成 float 再乘（会双重舍入差 1 ulp）
Vec3 polar(float angle, float length) noexcept {
    return {float(std::cos(double(angle)) * double(length)), float(std::sin(double(angle)) * double(length)), 0.0f};
}

float atan2_f(float y, float x) noexcept { return float(std::atan2(double(y), double(x))); }

Vec3 normalize3(const Vec3& v) noexcept {
    const float len = std::sqrt((v.x * v.x + v.y * v.y) + v.z * v.z);
    if (len == 0.0f) return v;
    return {v.x / len, v.y / len, v.z / len};
}

namespace {
inline void polar_into(float angle, float length, float& x, float& y) noexcept {
    const Vec3 p = polar(angle, length);
    x = p.x;
    y = p.y;
}
}  // namespace

void MotionState::update_velocity(float game_speed) noexcept {
    switch (mode & 0xf) {
    case 0:
        polar_into(angle, speed * game_speed, velocity.x, velocity.y);
        break;
    case 2:
    case 3:
        radius = radius_speed * game_speed + radius;
        angle = wrap_angle(speed * game_speed + angle);
        break;
    case 4:
        // 0x405420(+0x30) + 极坐标(angle2) → velocity；+0x3c = 0 —— 敌弹移植时补
        break;
    default:
        break;
    }
}

void MotionState::integrate(float game_speed) noexcept {
    (void)game_speed;
    switch (mode & 0xf) {
    case 0:
        pos.x += velocity.x;
        pos.y += velocity.y;
        pos.z += velocity.z;
        break;
    case 2: {
        float lx, ly;
        polar_into(angle, radius, lx, ly);
        pos.x = lx + velocity.x;
        pos.y = velocity.y + ly;
        pos.z = velocity.z + 0.0f;
        break;
    }
    case 3:
    case 4:
        // 0x405390 角差 / 轨道积分 —— 敌弹移植时补
        break;
    default:
        break;
    }
    pos.x = floor_f(pos.x * 100.0f) / 100.0f;
    pos.y = floor_f(pos.y * 100.0f) / 100.0f;
}

}  // namespace th16
