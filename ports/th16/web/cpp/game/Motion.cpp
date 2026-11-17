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

namespace {
// 0x4054d0：x87 fcos/fsin（扩展精度）× 长度后才舍入到单精度——不能先把 cos 舍成 float 再乘（会双重舍入差 1 ulp）
inline void polar(float angle, float length, float& x, float& y) noexcept {
    x = float(std::cos(double(angle)) * double(length));
    y = float(std::sin(double(angle)) * double(length));
}
}  // namespace

void MotionState::update_velocity(float game_speed) noexcept {
    switch (mode & 0xf) {
    case 0:
        polar(angle, speed * game_speed, velocity.x, velocity.y);
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
        polar(angle, radius, lx, ly);
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
