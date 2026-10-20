// 字体设备（桩实现：本轮全部文本走 ascii.anm 的 AsciiText 通道；
// th13 字号/字重槽集待按原版逐槽校准后接入 RuntimeFontRaster，ui-base.md §八.3）。
#pragma once

#include "GraphicsDevice.hpp"

#include <string>

namespace th13::sdl {

class FontDevice {
public:
    std::string error;
    explicit FontDevice(GraphicsDevice&) {}
    bool initialize() { return true; }  // 桩：对话/唱片室文本分期接入
    i32 extent(const std::string&, i32) { return -1; }
};

}  // namespace th13::sdl
