// th16 键位表——原版 DirectInput 扫描码（TitleScene+0x5b34 / 0x44f710）与宿主 SDL_Scancode 的对照。
// KeyboardMap.inc 第三列为 DI 扫描码；可配置槽：Shot / Bomb / Slow / Skip / Release。
#pragma once
#include "Input.hpp"
#include "Types.hpp"

namespace th16 {

inline constexpr int kConfigurableBinds = 5;

// DI → 逻辑位（可配置 5 键）；方向 / Esc / Enter 等仍由宿主固定映射
inline u32 key_bind_bit(int slot) {
    switch (slot) {
    case 0: return InputState::kShot;
    case 1: return InputState::kBomb;
    case 2: return InputState::kSlow;
    case 3: return InputState::kSkip;
    case 4: return InputState::kRelease;
    default: return 0;
    }
}

#if defined(__EMSCRIPTEN__) || defined(TH16_HAS_SDL)
#include <SDL3/SDL.h>

// DirectInput 扫描码 → SDL_Scancode（可配置键 + 捕获常用字母区）
inline SDL_Scancode di_to_sdl(i16 di) {
    switch (di & 0xff) {
    case 0x01: return SDL_SCANCODE_ESCAPE;
    case 0x1c: return SDL_SCANCODE_RETURN;
    case 0x1d: return SDL_SCANCODE_LCTRL;
    case 0x2a: return SDL_SCANCODE_LSHIFT;
    case 0x36: return SDL_SCANCODE_RSHIFT;
    case 0x38: return SDL_SCANCODE_LALT;
    case 0x9d: return SDL_SCANCODE_RCTRL;
    case 0x2c: return SDL_SCANCODE_Z;
    case 0x2d: return SDL_SCANCODE_X;
    case 0x2e: return SDL_SCANCODE_C;
    case 0x1e: return SDL_SCANCODE_A;
    case 0x1f: return SDL_SCANCODE_S;
    case 0x20: return SDL_SCANCODE_D;
    case 0x11: return SDL_SCANCODE_W;
    case 0x10: return SDL_SCANCODE_Q;
    case 0x12: return SDL_SCANCODE_E;
    case 0x13: return SDL_SCANCODE_R;
    case 0x14: return SDL_SCANCODE_T;
    case 0x15: return SDL_SCANCODE_Y;
    case 0x16: return SDL_SCANCODE_U;
    case 0x17: return SDL_SCANCODE_I;
    case 0x18: return SDL_SCANCODE_O;
    case 0x19: return SDL_SCANCODE_P;
    case 0x21: return SDL_SCANCODE_F;
    case 0x22: return SDL_SCANCODE_G;
    case 0x23: return SDL_SCANCODE_H;
    case 0x24: return SDL_SCANCODE_J;
    case 0x25: return SDL_SCANCODE_K;
    case 0x26: return SDL_SCANCODE_L;
    case 0x2f: return SDL_SCANCODE_V;
    case 0x30: return SDL_SCANCODE_B;
    case 0x31: return SDL_SCANCODE_N;
    case 0x32: return SDL_SCANCODE_M;
    case 0x39: return SDL_SCANCODE_SPACE;
    case 0xc8: return SDL_SCANCODE_UP;
    case 0xd0: return SDL_SCANCODE_DOWN;
    case 0xcb: return SDL_SCANCODE_LEFT;
    case 0xcd: return SDL_SCANCODE_RIGHT;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

inline i16 sdl_to_di(SDL_Scancode s) {
    switch (s) {
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return 0x1c;
    case SDL_SCANCODE_LCTRL: return 0x1d;
    case SDL_SCANCODE_RCTRL: return 0x9d;
    case SDL_SCANCODE_LSHIFT: return 0x2a;
    case SDL_SCANCODE_RSHIFT: return 0x36;
    case SDL_SCANCODE_LALT: return 0x38;
    case SDL_SCANCODE_Z: return 0x2c;
    case SDL_SCANCODE_X: return 0x2d;
    case SDL_SCANCODE_C: return 0x2e;
    case SDL_SCANCODE_A: return 0x1e;
    case SDL_SCANCODE_S: return 0x1f;
    case SDL_SCANCODE_D: return 0x20;
    case SDL_SCANCODE_W: return 0x11;
    case SDL_SCANCODE_Q: return 0x10;
    case SDL_SCANCODE_E: return 0x12;
    case SDL_SCANCODE_R: return 0x13;
    case SDL_SCANCODE_T: return 0x14;
    case SDL_SCANCODE_Y: return 0x15;
    case SDL_SCANCODE_U: return 0x16;
    case SDL_SCANCODE_I: return 0x17;
    case SDL_SCANCODE_O: return 0x18;
    case SDL_SCANCODE_P: return 0x19;
    case SDL_SCANCODE_F: return 0x21;
    case SDL_SCANCODE_G: return 0x22;
    case SDL_SCANCODE_H: return 0x23;
    case SDL_SCANCODE_J: return 0x24;
    case SDL_SCANCODE_K: return 0x25;
    case SDL_SCANCODE_L: return 0x26;
    case SDL_SCANCODE_V: return 0x2f;
    case SDL_SCANCODE_B: return 0x30;
    case SDL_SCANCODE_N: return 0x31;
    case SDL_SCANCODE_M: return 0x32;
    case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_UP: return 0xc8;
    case SDL_SCANCODE_DOWN: return 0xd0;
    case SDL_SCANCODE_LEFT: return 0xcb;
    case SDL_SCANCODE_RIGHT: return 0xcd;
    default: return -1;
    }
}
#endif

}  // namespace th16
