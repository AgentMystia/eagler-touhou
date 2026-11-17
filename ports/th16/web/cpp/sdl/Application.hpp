// th16 应用：从 MEMFS /assets/th16.dat 打开 THA1 档案，驱动场景帧并提交到 GraphicsDevice。
// 启动幕（原版 0x43adc0 / 0x43b300）：装载 sig.anm → AsciiManager 槽 → spawn script0（layer -1）→
// 每帧 tick_world（0x46e340）+ 按 layer 链绘制（0x46e750 / 0x468490）。
#pragma once
#include "GraphicsDevice.hpp"
#include "../game/AnmManager.hpp"
#include "../game/AnmRenderer.hpp"
#include "../game/Archive.hpp"
#include "../game/AnmResource.hpp"

#include <memory>
#include <string>
#include <vector>

namespace th16::sdl {

class Application {
public:
    GraphicsDevice graphics;
    AnmManager animations;
    AnmRenderer renderer;
    std::string failure;
    unsigned frames = 0;
    bool initialized = false;

    bool initialize(const char* archive_path);
    // 一帧：更新 + 绘制 + present。返回 false = 致命错误（failure 已填）。
    bool step();

private:
    std::vector<u8> archive_bytes;
    Archive archive;
    AnmResource ascii;
    AnmResource sig;
    bool load_anm(const char* name, AnmResource& out);
    void draw_frame();
};

}  // namespace th16::sdl
