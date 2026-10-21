// th13 弹幕外观（类型 → 脚本/精灵表）。表体见 BulletAppearance.cpp 文件头：
// 数据逐字节取自原版 .data，弹体精灵解析与 0x4120b0 回调语义一致。
#pragma once

#include "Types.hpp"

#include <array>

namespace th13 {

struct BulletAppearance {
    i32 script;  // 弹体 ANM 脚本（bullet.anm 内编号）
    struct Color {
        i32 sprite, hit, spawn, cancel;  // 逻辑参数 0..3 → 精灵
    };
    std::array<Color, 16> colors;
    float scale;  // 碰撞半径推导用缩放（+0x104）
    i32 hit_kind, cancel_kind, spawn_script;
};

const BulletAppearance* bullet_appearance(i32 type) noexcept;

// 0x4120b0：类型未映射（首色弹体精灵 <0）时原样返回参数。
inline i32 bullet_animation_parameter(i32 type, i32 color, i32 parameter) noexcept {
    const auto* entry = bullet_appearance(type);
    if (!entry || color < 0 || color >= 16 || entry->colors[0].sprite < 0) return parameter;
    const auto& value = entry->colors[u32(color)];
    switch (parameter) {
    case 0: return value.sprite;
    case 1: return value.hit;
    case 2: return value.spawn;
    case 3: return value.cancel;
    default: return parameter;
    }
}

}  // namespace th13
