#include "FontPainter.hpp"

#include "GraphicsDevice.hpp"

#include <algorithm>
#include <cstring>

#if defined(TH16_HAS_TTF)
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vector>
#endif

namespace th16::sdl {
namespace {

#if defined(TH16_HAS_TTF)
SDL_Color bgr_to_rgba(u32 colorref, u8 alpha = 255) {
    return SDL_Color{u8(colorref & 0xff), u8((colorref >> 8) & 0xff), u8((colorref >> 16) & 0xff), alpha};
}

const char* pick_font() {
    static const char* kCandidates[] = {
        "/fonts/msgothic.ttc",
        "/msgothic.ttc",
        "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        nullptr,
    };
    for (const char** p = kCandidates; *p; ++p) {
        SDL_IOStream* io = SDL_IOFromFile(*p, "rb");
        if (io) {
            SDL_CloseIO(io);
            return *p;
        }
    }
    return nullptr;
}

// CP932 → Unicode（ASCII + iconv 回退到逐字节）
std::vector<u32> decode_cp932(const std::string& text) {
    std::vector<u32> out;
    out.reserve(text.size());
    const u8* p = reinterpret_cast<const u8*>(text.data());
    const u8* end = p + text.size();
    while (p < end) {
        u32 c = *p++;
        if (c < 0x80) {
            out.push_back(c);
            continue;
        }
        if (((c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc)) && p < end) {
            // 粗略：交给字体的私用区不可靠；无完整表时用 raw 双字节当 PUA 跳过，画一个中点
            ++p;
            out.push_back(0x30fb);
            continue;
        }
        out.push_back(c);
    }
    return out;
}
#endif

}  // namespace

bool FontPainter::initialize() {
    if (ready_) return true;
#if defined(TH16_HAS_TTF)
    if (!TTF_Init()) {
        error = "TTF_Init failed";
        return false;
    }
    const char* path = pick_font();
    if (path == nullptr) {
        error = "no CJK font available for text-into-texture";
        TTF_Quit();
        return false;
    }
    font_path_ = path;
    ready_ = true;
    return true;
#else
    // 网页默认构建尚未接 sdl3_ttf：只清矩形，待 TH16_HAS_TTF 打开后再画字形
    ready_ = true;
    return true;
#endif
}

bool FontPainter::flush(GraphicsDevice& graphics, AnmManager& animations, std::vector<TextIntoTextureRequest>& queue) {
    if (queue.empty()) return true;
    if (!initialize()) {
        queue.clear();
        return true;
    }
    for (const TextIntoTextureRequest& req : queue) {
        AnmVm* vm = animations.find(req.vm_id);
        if (vm == nullptr) continue;
        const i32 slot = vm->sprite_source_slot >= 0 ? vm->sprite_source_slot : vm->file_slot;
        AnmFile* file = animations.file(slot);
        if (file == nullptr || file->resource == nullptr || vm->sprite_index < 0 ||
            u32(vm->sprite_index) >= file->sprites.size())
            continue;
        const AnmRuntimeSprite& spr = file->sprites[u32(vm->sprite_index)];
        const u32 handle = graphics.texture(*file->resource, u32(spr.entry_index));
        if (handle == 0) continue;

        const i32 left = req.left, top = req.top, right = req.right, bottom = req.bottom;
        if (right <= left || bottom <= top) continue;
        graphics.clear_texture_rect(handle, left, top, right, bottom);

#if defined(TH16_HAS_TTF)
        const i32 height = std::max(17, text_font_height(req.font));
        TTF_Font* font = TTF_OpenFont(font_path_.c_str(), float(height));
        if (font == nullptr) continue;
        TTF_SetFontStyle(font, height >= 21 ? TTF_STYLE_BOLD : TTF_STYLE_NORMAL);
        const auto codes = decode_cp932(req.text);
        i32 pen_x = left + (req.centered ? 0 : req.x * 2 + 2);
        i32 pen_y = top + 2;
        i32 total_w = 0;
        struct GlyphBlit {
            i32 x, y, w, h;
            std::vector<u8> rgba;
        };
        std::vector<GlyphBlit> glyphs;
        for (u32 code : codes) {
            SDL_Surface* surface = TTF_RenderGlyph_Blended(font, code, bgr_to_rgba(req.color));
            if (surface == nullptr) continue;
            SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(surface);
            if (rgba == nullptr) continue;
            GlyphBlit g;
            g.w = rgba->w;
            g.h = rgba->h;
            g.rgba.resize(size_t(rgba->pitch) * rgba->h);
            std::memcpy(g.rgba.data(), rgba->pixels, g.rgba.size());
            g.x = total_w;
            g.y = 0;
            total_w += rgba->w;
            glyphs.push_back(std::move(g));
            SDL_DestroySurface(rgba);
        }
        if (req.centered) pen_x = left + (right - left - total_w) / 2;
        for (const GlyphBlit& g : glyphs) {
            graphics.blit_rgba_to_texture(handle, pen_x + g.x, pen_y + g.y, g.rgba.data(), g.w, g.h, g.w * 4);
        }
        TTF_CloseFont(font);
#endif
        vm->flags_lo |= 1u;
    }
    queue.clear();
    return true;
}

}  // namespace th16::sdl
