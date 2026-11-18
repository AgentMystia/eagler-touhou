// th16 ScreenEffect——原版 0x45d150（0x40 字节，operator new）/ 0x45d1a0 按 kind 登记更新链 0x13 与绘制链 param_6。
//
//   kind 1（ECL 517 画面震动，绘制链 0x54）：0x45cd30 每帧计时后按 (to - from) × t / duration + from 取振幅，
//   用 game_rng（DAT_004a6d80）各双步 0x402be0 一次、无符号 % 3 决定 x / y 的 0 / +amp / -amp，写 DAT_004c1788/178c（逻辑偏移）
//   与 DAT_004c1530/1534（× 窗口缩放 DAT_004d9d34）；计时达 duration 返回 7 → 0x45d130 摘除两条链并释放。
//   kind 0 / 2 / 3 / 5（淡入淡出，更新链 0x45c900、绘制链 0x45c990 全屏色块）：DAT_004c0f40 非零时直接返回 7 自删；
//   否则 alpha = t × 255 / duration（到时 0xff），t < duration + 2 期间在 GameManager 非装载 / 非结束时推进计时；
//   换关 0x40c0d0 建 kind 2（0x1e 帧、色 0），过关结算 0x42d7b0 建 kind 5（200 帧）。
//   其余 kind（4/6/7/9 等纯绘制）渲染侧接入时补。
//
// 震动消耗 game_rng 与敌弹共用一条随机序列，因此必须与原版同帧同序执行（链 0x13 在敌机链 0x1a 之前）。
#pragma once
#include "AnmVm.hpp"
#include "Rng.hpp"
#include "Types.hpp"

namespace th16 {

struct ScreenShake {
    i32 duration = 0;     // +0x18
    i32 amp_from = 0;     // +0x1c
    i32 amp_to = 0;       // +0x20
    Timer timer;          // +0x2c..+0x3c
};

// 0x45cd30 的全局输出（DAT_004c1788/178c、DAT_004c1530/1534）
struct ScreenShakeOffset {
    Vec2 offset;         // DAT_004c1788 / 178c
    Vec2 offset_scaled;  // DAT_004c1530 / 1534
};

// 0x45cd30：推进一帧；返回 false 表示计时结束（原版返回 7 自删）
bool screen_shake_step(ScreenShake& shake, Rng& game_rng, float window_scale, float game_speed,
                       ScreenShakeOffset& out);

struct ScreenFade {
    i32 kind = 0;         // +0x0c
    i32 alpha = 0;        // +0x14：0x45c990 以 alpha << 24 | color 画全屏
    i32 duration = 0;     // +0x18
    u32 color = 0;        // +0x1c
    Timer timer;          // +0x2c..+0x3c
};

// 0x45c900：推进一帧；game_frozen = GameManager 装载中或已结束（(flags >> 2 | flags) & 1）；返回 false 表示自删
bool screen_fade_step(ScreenFade& fade, i32 post_load_frames, bool game_frozen, float game_speed);

}  // namespace th16
