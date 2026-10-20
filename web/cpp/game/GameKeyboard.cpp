#include "GameKeyboard.hpp"

namespace th13 {

u32 keyboard_keys(const bool* keys) noexcept {
    if (!keys) return 0;
    u32 value = 0;
    // 本轮接入位（ui-base.md §六）：射击/炸弹/低速/移动（含小键盘与斜向）/暂停/快进/确认。
    constexpr std::pair<u32, u32> bindings[] = {
        {90, 1},        // Z：射击/确认
        {88, 2},        // X：炸弹/取消
        {16, 8},        // Shift：低速移动（双 Shift 在 KeyboardMap 层归一为 vk16）
        {38, 16},       // ↑
        {104, 16},      // 小键盘 8
        {40, 32},       // ↓
        {98, 32},       // 小键盘 2
        {37, 64},       // ←
        {100, 64},      // 小键盘 4
        {39, 128},      // →
        {102, 128},     // 小键盘 6
        {97, 0x60},     // 小键盘 7：左上
        {99, 0xa0},     // 小键盘 1：左下
        {103, 0x50},    // 小键盘 9：右上
        {105, 0x90},    // 小键盘 3：右下
        {27, 0x100},    // Esc：暂停菜单
        {17, 0x200},    // Ctrl：对话快进（对话为桩时仅留位）
        {13, 0x80000},  // Enter：菜单确认
    };
    // 暂不接入位（保留原版位值，接线时机见分期）：
    //   67(C)=0xa00 回放快慢放；81(Q)=0x10000 中途退出；83(S)=0x20000 帧跳过；
    //   36/80=0x40000 截图；68(D)=0x100000 回放调试；82(R)=0x200000 快速重试；
    //   121(F10)=0x800000 帧率显示。
    for (const auto& binding : bindings)
        if (keys[binding.first]) value |= binding.second;
    return value;
}

}  // namespace th13
