#include "FontDevice.hpp"
#include "../game/MsgPlayer.hpp"  // build_cp932_font_mapping（仓内 cp932 表）

namespace th13::sdl {

// 原版对话日文栅格走 text.anm 动态纹理（0x4282b0 建 scripts0/1/2/3 正文/ruby
// VM；0x46e8f0 逐字消费）——本设备是该链路的栅格源，结构与 th15 FontDevice
// 同族（eagler::text::RuntimeFontRaster + SDL3_ttf）。
//
// cp932 映射：th15 经 /fonts/cp932.bin 启动资产；th13 在仓内由
// MsgPlayer 的 cp932 表推导（同 131072 字节 u16le 语义），不再要求 VFS 文件。
//
// 字体资产边界（本轮实测）：原版 th13 不自带字体文件（0x46f6a0 CreateFontA
// 面用系统 MS Gothic）；容器与仓库（games/、artifacts/、Wine 前缀）均无
// msgothic.ttc，th15 侧烘焙字体 .bin 也未随仓（仅 manifest.json）。红线禁止
// 以系统/网络字体替代制造「看起来像」——故 TTF 面自举失败（/msgothic.ttc
// 与 /fonts/msgothic.ttc 均缺席）时 initialize 显式失败，字体资产来源（授权
// 渠道/打包方式）留给具名授权任务。资产就位（VFS 根或 /fonts 放真实
// msgothic.ttc）后本文件无需再改。
bool FontDevice::initialize() {
    if (ready_) return true;
    if (!build_cp932_font_mapping(mapping_) || mapping_.size() != 131072) {
        error = "TH13 dialogue cp932 mapping derivation failed (in-repo table)";
        return false;
    }
    if (!raster.initialize(mapping_.data(), mapping_.size())) {
        error =
            "TH13 dialogue font unavailable: original MS Gothic (/msgothic.ttc or "
            "/fonts/msgothic.ttc) is absent from the VFS; Japanese dialogue text "
            "rendering stays BLOCKED (no substitute font per asset red line; see "
            "artifacts/verification/r4/dialogue/HANDOFF.md)";
        return false;
    }
    ready_ = true;
    return true;
}

i32 FontDevice::extent(const std::string& value, i32 font) {
    // 查宽面待对话文本 VM（0x46e8f0）接线后按原版字号/字重槽校准；栅格面
    // 未接线期恒 -1（调用方不假装可得）。
    (void)value;
    (void)font;
    if (!ready_) return -1;
    return -1;
}

}  // namespace th13::sdl
