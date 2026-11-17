// th16 应用：从 MEMFS /assets/th16.dat 打开 THA1 档案，驱动场景帧并提交到 GraphicsDevice。
// 当前阶段 = 启动幕（原版 0x43adc0：装载 sig.anm → text.anm → thbgm.fmt → score）的静态首帧；
// ANM VM 落地后由 VM 执行 sig.anm script0/1 取代这里手写的精灵摆放。
#pragma once
#include "GraphicsDevice.hpp"
#include "../game/Archive.hpp"
#include "../game/AnmResource.hpp"

#include <memory>
#include <string>
#include <vector>

namespace th16::sdl {

class Application {
public:
    GraphicsDevice graphics;
    std::string failure;
    unsigned frames = 0;
    bool initialized = false;

    bool initialize(const char* archive_path);
    // 一帧：更新 + 绘制 + present。返回 false = 致命错误（failure 已填）。
    bool step();

private:
    std::vector<u8> archive_bytes;
    Archive archive;
    AnmResource sig;
    bool load_anm(const char* name, AnmResource& out);
    void draw_boot_frame();
};

}  // namespace th16::sdl
