// th16 应用：打开 th16.dat，驱动 Supervisor 场景帧并提交到 GraphicsDevice。
// 启动幕（0x43adc0 / 0x43b300）→ 标题（0x44aee0 / 0x44abc0 等待 0xb4 帧）时序见 game/Supervisor.hpp。
#pragma once
#include "GraphicsDevice.hpp"
#include "../game/AnmManager.hpp"
#include "../game/AnmRenderer.hpp"
#include "../game/AnmResource.hpp"
#include "../game/Archive.hpp"
#include "../game/Supervisor.hpp"

#include <string>
#include <vector>

namespace th16::sdl {

class Application {
public:
    GraphicsDevice graphics;
    AnmManager animations;
    AnmRenderer renderer;
    Supervisor supervisor;
    std::string failure;
    unsigned frames = 0;
    bool initialized = false;

    bool initialize(const char* archive_path);
    // 一帧：输入 → 场景切换 → tick → 绘制。返回 false = 致命错误。
    bool step();

private:
    std::vector<u8> archive_bytes;
    Archive archive;
    AnmResource ascii;
    AnmResource sig;
    AnmResource title;
    AnmResource title_v;

    bool load_anm(const char* name, AnmResource& out);
    bool enter_boot();
    bool enter_title_assets();   // 装载 title*.anm（仍显示启动幕）
    bool activate_title_menu();  // 卸 sig、生成菜单脚本（0x44b5f0 入口子集）
    void sample_input();
    void draw_frame();
};

}  // namespace th16::sdl
