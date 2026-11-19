#include "TextPaint.hpp"

#include "AnmManager.hpp"

namespace th16 {
namespace {

bool sprite_rect(AnmManager& animations, const AnmVm& vm, i32& left, i32& top, i32& right, i32& bottom, float* width_out) {
    const i32 slot = vm.sprite_source_slot >= 0 ? vm.sprite_source_slot : vm.file_slot;
    AnmFile* file = animations.file(slot);
    if (file == nullptr || vm.sprite_index < 0 || u32(vm.sprite_index) >= file->sprites.size()) return false;
    const AnmRuntimeSprite& s = file->sprites[u32(vm.sprite_index)];
    left = i32(s.x0);
    top = i32(s.y0);
    right = i32(s.x1);
    bottom = i32(s.y1);
    if (width_out) *width_out = s.width;
    return true;
}

u32 effective_outline(const AnmVm& vm, u32 outline) {
    // 0x46d990：flags_hi & 0x1000 → 阴影色清零
    if ((vm.flags_hi & 0x1000u) != 0) return 0;
    return outline;
}

}  // namespace

bool text_paint_vm(AnmManager& animations, AnmVm& vm, u32 color, u32 outline, i32 font, i32 x, i32 y,
                   const char* text, std::vector<TextIntoTextureRequest>* out) {
    if (text == nullptr) text = "";
    const i32 height = text_font_height(font);
    // 原版：字号 ∈ [1,8] 时跳过 GDI 写入，但仍置 flags_lo bit0
    i32 left = 0, top = 0, right = 0, bottom = 0;
    const bool have_rect = sprite_rect(animations, vm, left, top, right, bottom, nullptr);
    vm.flags_lo |= 1u;
    if (!have_rect) return false;
    if (height >= 1 && height < 9) return true;
    if (out == nullptr) return true;
    TextIntoTextureRequest req;
    req.vm_id = vm.id;
    req.text = text;
    req.color = color;
    req.outline = effective_outline(vm, outline);
    req.font = font;
    req.x = x;
    req.y = y;
    req.centered = false;
    req.left = left;
    req.top = top;
    req.right = right;
    req.bottom = bottom;
    out->push_back(std::move(req));
    return true;
}

bool text_paint_vm_centered(AnmManager& animations, AnmVm& vm, u32 color, u32 outline, i32 font, i32 shadow,
                            const char* text, std::vector<TextIntoTextureRequest>* out) {
    if (text == nullptr) text = "";
    const i32 height = text_font_height(font);
    i32 left = 0, top = 0, right = 0, bottom = 0;
    float sprite_w = 0;
    const bool have_rect = sprite_rect(animations, vm, left, top, right, bottom, &sprite_w);
    vm.flags_lo |= 1u;
    if (!have_rect) return false;
    if (height >= 1 && height < 9) return true;
    // 0x46dab0：x = sprite.width − ((font_h*2-1) * char_count)/2 … 由宿主按字形宽度再算；
    // 这里先把居中意图与阴影开关传出，像素侧用 measure 精算。
    if (out == nullptr) return true;
    TextIntoTextureRequest req;
    req.vm_id = vm.id;
    req.text = text;
    req.color = color;
    req.outline = effective_outline(vm, outline);
    req.font = font;
    req.x = i32(sprite_w);
    req.y = shadow;
    req.centered = true;
    req.left = left;
    req.top = top;
    req.right = right;
    req.bottom = bottom;
    out->push_back(std::move(req));
    return true;
}

}  // namespace th16
