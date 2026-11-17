// th16 实体运动状态（0x44 字节）：自机弹 +0x48、敌弹、道具等共用。
//   0x402ff0  按模式算速度分量（mode 0：极坐标 → velocity；mode 2/3：角速度/半径推进；mode 4：轨道）
//   0x403110  按模式推进位置，再把 x/y 截到 1/100（×100 → floor → ÷100，单精度乘除、CRT floor）
// 本阶段只实现自机弹用到的 mode 0 与 mode 2；mode 3/4 由敌弹（0x411dd0）移植时随其 oracle 补齐。
#pragma once
#include "Types.hpp"

namespace th16 {

// 0x402d90：把角度折回 (-π, π]（两段循环共用计数器，各至多 0x22 次）
float wrap_angle(float a) noexcept;
// 0x405510 / 0x4054f0：CVTSS2SD → CRT sin/cos(double) → CVTSD2SS
float sin_f(float a) noexcept;
float cos_f(float a) noexcept;
// 0x405260：CVTSS2SD → CRT floor → CVTSD2SS
float floor_f(float a) noexcept;

struct MotionState {
    Vec3 pos;              // +0x00
    Vec3 pos2;             // +0x0c（mode 4：积分位置）
    float speed = 0;       // +0x18（mode 2/3：角速度）
    float angle = 0;       // +0x1c
    float radius = 0;      // +0x20（mode 2/3）
    float radius_speed = 0;// +0x24
    float angle2 = 0;      // +0x28（mode 4）
    float f2c = 0;         // +0x2c
    float f30 = 0;         // +0x30
    Vec3 velocity;         // +0x34（mode 2：圆心）
    u32 mode = 0;          // +0x40 低 4 位为模式

    // 0x402ff0
    void update_velocity(float game_speed) noexcept;
    // 0x403110
    void integrate(float game_speed) noexcept;
};
static_assert(sizeof(MotionState) == 0x44, "MotionState 为 0x44 字节");

}  // namespace th16
