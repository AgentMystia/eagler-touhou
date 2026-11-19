// th16 ScreenEffect——原版 0x45d150（0x40 字节，operator new）/ 0x45d1a0 按 kind 登记更新链 0x13 与绘制链 param_6。
//
//   kind 1（ECL 517 画面震动，绘制链 0x54）：0x45cd30 每帧计时后按 (to - from) × t / duration + from 取振幅，
//   用 game_rng（DAT_004a6d80）各双步 0x402be0 一次、无符号 % 3 决定 x / y 的 0 / +amp / -amp，写 DAT_004c1788/178c（逻辑偏移）
//   与 DAT_004c1530/1534（× 窗口缩放 DAT_004d9d34）；计时达 duration 返回 7 → 0x45d130 摘除两条链并释放。
//   kind 2 / 5（淡出到色块，更新链 0x45c900、绘制链 0x45c990 / 0x45c860）：DAT_004c0f40 非零时直接返回 7 自删；
//   否则 alpha = t × 255 / duration（到时 0xff），t < duration + 2 期间在 GameManager 非装载 / 非结束时推进计时；
//   换关 0x40c0d0 建 kind 2（0x1e 帧、色 0），过关结算 0x42d7b0 建 kind 5（200 帧）。
//   kind 0 / 3（从色块淡入，更新链 0x45c630、绘制链 0x45c990）：alpha = 255 − t × 255 / duration（kind 3 初值 0xff），
//   t ≥ duration 时自删，否则无条件推进计时；关卡切换淡入 0x409f90 建 kind 3（0x1e 帧、白）。
//   kind 8（炸弹震动，0x45cf10）：振幅按 attack（线性升）/ sustain（满幅）/ decay（线性降）三段包络，
//   用 visual_rng（DAT_004a6d88）各双步一次决定 x / y 方向；DAT_004c0f40 非零时自删，GameManager 装载 / 结束 /
//   flags & 0x72 时跳过本帧；超过三段总长自删。
//   kind 9（标题返回练习 / 符卡练习：0x44af80 在 title_origin 4 / 5 时建，0x1e 帧、色 0、绘制链 0x54）：
//   更新 0x45cac0（alpha 恒 0xff，t ≥ duration 自删）；绘制 0x45cb50（不改视口，直接 0x45c690 整幅填色）。
//   其余 kind（4/6/7）渲染侧接入时补。
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

// kind 8：炸弹震动包络（0x45d150(8) 由 0x40e780 / 0x40f070 / 0x40f930 创建；0x45cf10 推进）
struct ScreenShakeEnvelope {
    i32 amplitude = 0;    // +0x18
    i32 attack = 0;       // +0x1c：振幅 0 → amplitude 的帧数
    i32 sustain = 0;      // +0x20：满幅帧数
    i32 decay = 0;        // +0x24：amplitude → 0 的帧数
    Timer timer;          // +0x2c..+0x3c
};

// 0x45cf10：推进一帧；返回 false 表示应自删（原版返回 7）。game_frozen = GameManager 装载 / 结束 / flags & 0x72
bool screen_shake_envelope_step(ScreenShakeEnvelope& shake, Rng& visual_rng, float window_scale, float game_speed,
                                i32 post_load_frames, bool game_frozen, ScreenShakeOffset& out);

struct ScreenFade {
    i32 kind = 0;         // +0x0c
    i32 alpha = 0;        // +0x14：0x45c990 以 alpha << 24 | color 画全屏
    i32 duration = 0;     // +0x18
    u32 color = 0;        // +0x1c
    Timer timer;          // +0x2c..+0x3c
};

// 0x45c900（kind 2 / 5）：推进一帧；game_frozen = GameManager 装载中或已结束（(flags >> 2 | flags) & 1）；返回 false 表示自删
bool screen_fade_step(ScreenFade& fade, i32 post_load_frames, bool game_frozen, float game_speed);
// 0x45c630（kind 0 / 3）：alpha 由 255 递减；计时到 duration 返回 false
bool screen_fade_in_step(ScreenFade& fade, i32 post_load_frames, float game_speed);
// 0x45cac0（kind 9）：alpha 恒 0xff；计时到 duration 返回 false
bool screen_fade_hold_step(ScreenFade& fade, i32 post_load_frames, float game_speed);

}  // namespace th16
