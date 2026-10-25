// 字体设备（对话/日文文本栅格源）：结构与 th15 同族——SDL3_ttf +
// eagler::text::RuntimeFontRaster（common/include/eagler/text）。
// cp932 映射在仓内推导（game/MsgPlayer 的 build_cp932_font_mapping），
// 不要求 /fonts/cp932.bin 启动资产；真实字体文件（MS Gothic）缺席时
// initialize 显式失败（BLOCKED，不伪造、不换字体），见 FontDevice.cpp 头注。
// 字号/字重槽集待 0x4282b0/0x46e8f0 校准后扩展。
#pragma once

#include "GraphicsDevice.hpp"
#include <eagler/text/RuntimeFontRaster.hpp>

#include <string>
#include <vector>

namespace th13::sdl {

class FontDevice {
public:
    std::string error;
    explicit FontDevice(GraphicsDevice&) {}
    bool initialize();  // 字体文件缺席 = false + error（显式 BLOCKED 面）
    bool ready() const noexcept { return ready_; }
    i32 extent(const std::string&, i32);  // 查宽：栅格面接线前恒 -1

private:
    // 字号槽集按原版 0x46f6a0 CreateFontA 参数族补齐（当前仅占位单槽）。
    eagler::text::RuntimeFontRaster raster{{24, 400}};
    std::vector<u8> mapping_;  // 131072 字节 cp932→Unicode（仓内推导）
    bool ready_ = false;
};

}  // namespace th13::sdl
