// th16 宿主键盘对照——SDL_Scancode ↔ DirectInput 扫描码（KeyboardMap.inc 第三列）。
// 原版 0x401d50 的键位是固定的（Key Config 只配手柄按钮，见 GameGlobals::pad_buttons）；这里的 DI 码只喂给
// 0x402130 键盘快照（TitleScene::keyboard_state：Player Data 隐藏键序等）。
#pragma once
#include "Types.hpp"

#if defined(__EMSCRIPTEN__) || defined(TH16_HAS_SDL)
#include <SDL3/SDL.h>
#endif

namespace th16 {

#if defined(__EMSCRIPTEN__) || defined(TH16_HAS_SDL)
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
