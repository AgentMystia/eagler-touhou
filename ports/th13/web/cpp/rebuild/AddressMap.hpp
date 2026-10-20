// 原版 th13.exe（v1.00c, PE 时间戳 2011-08-17）地址锚点表。
// 每完成一个子系统的行为还原，就在此登记它的原版入口与 C++ 对应物，
// 使 oracle 对照与回归脚本可以直接引用，不再散落硬编码地址。
#pragma once

#include <cstdint>

namespace th13::rebuild {

constexpr uint32_t kImageBase = 0x400000;
constexpr uint32_t kEntryPoint = kImageBase + 0x1BB2F1;   // 待 Ghidra 导出后校正

// 还原进度登记：子系统名 → 原版入口地址（0 = 未定位/未还原）
struct AddressAnchor {
    const char* subsystem;
    uint32_t originalEntry;
};

constexpr AddressAnchor kAnchors[] = {
    {"rng", 0},          // 随机数
    {"supervisor", 0},   // 主循环
    {"anm-vm", 0},       // 动画脚本虚拟机
    {"ecl-vm", 0},       // 敌机脚本虚拟机
    {"enemy-manager", 0},
    {"bullet-manager", 0},
    {"item-manager", 0},
    {"player", 0},
    {"spirit", 0},       // th13 特有：神灵/灵界系统
    {"replay", 0},
};

}  // namespace th13::rebuild
