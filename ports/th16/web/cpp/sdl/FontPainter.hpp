// th16 宿主文字刷入——对应原版 0x459240（GDI TextOut → DIB → D3DXLoadSurfaceFromMemory）。
// 无 msgothic / cp932 时回退系统 CJK 字体（DroidSansFallback / WenQuanYi）；仍按 CP932 字节当
// 本地编码交给 TTF（SDL 按字体 cmap 取字形）。逻辑侧已入队 TextIntoTextureRequest。
#pragma once
#include "../game/TextPaint.hpp"

#include <string>

namespace th16 {
class AnmManager;
}

namespace th16::sdl {

class GraphicsDevice;

class FontPainter {
    bool ready_ = false;
    std::string font_path_;

public:
    std::string error;
    bool initialize();
    // 把队列刷进 GraphicsDevice 纹理；成功后清空 queue
    bool flush(GraphicsDevice& graphics, AnmManager& animations, std::vector<TextIntoTextureRequest>& queue);
};

}  // namespace th16::sdl
