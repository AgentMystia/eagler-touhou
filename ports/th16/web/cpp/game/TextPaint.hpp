// th16 文字贴图——原版 0x46d990（对话行）/ 0x46dab0（符卡名居中）把 GDI TextOut 结果
// 经 0x459240 写入 ANM 纹理，并置 VM flags_lo bit0（0x530 |= 1）。
//
// 逻辑侧：格式化字符串、取精灵像素矩形（AnmRuntimeSprite +0x0c..+0x18）、置标志、入队请求。
// 像素写入由宿主（sdl::GraphicsDevice / FontPainter）在逻辑帧末执行；无字体时仍置标志并清空矩形，
// 避免旧字残留。
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "AnmVm.hpp"
#include "Types.hpp"

namespace th16 {

class AnmManager;
struct AnmFile;

struct TextIntoTextureRequest {
    u32 vm_id = 0;
    std::string text;
    u32 color = 0xffffff;     // COLORREF：0x00BBGGRR
    u32 outline = 0;          // 阴影 / 描边 COLORREF；flags_hi bit12 时强制 0
    i32 font = 0;             // 0..8 → DAT_00491b68 字号
    i32 x = 0;                // 0x46d990 param_6（调用 0x459240 前 ×2）
    i32 y = 0;                // 0x46d990 param_7（SJIS 字距 ×2）；0x46dab0 为阴影开关
    bool centered = false;    // 0x46dab0
    i32 left = 0, top = 0, right = 0, bottom = 0;  // 精灵像素矩形
};

// DAT_00491b68
inline i32 text_font_height(i32 font) noexcept {
    static constexpr i32 kHeights[] = {17, 17, 17, 17, 21, 21, 21, 21, 14};
    if (font < 0 || font > 8) return 0x11;
    return kHeights[font];
}

// 0x46d990：把格式化后的文字画进 vm 纹理（左对齐起点）；返回 false 表示 VM / 精灵无效
bool text_paint_vm(AnmManager& animations, AnmVm& vm, u32 color, u32 outline, i32 font, i32 x, i32 y,
                   const char* text, std::vector<TextIntoTextureRequest>* out);

// 0x46dab0：按精灵宽度居中
bool text_paint_vm_centered(AnmManager& animations, AnmVm& vm, u32 color, u32 outline, i32 font, i32 shadow,
                            const char* text, std::vector<TextIntoTextureRequest>* out);

}  // namespace th16
