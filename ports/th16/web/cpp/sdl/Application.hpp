// th16 应用：SDL/GLES 宿主——打开 th16.dat，驱动 GameSession（逻辑层）并把 ANM 四边形提交到 GraphicsDevice。
#pragma once
#include "GraphicsDevice.hpp"
#include "../game/AnmRenderer.hpp"
#include "../game/FrameClock.hpp"
#include "../game/GameSession.hpp"

#include <string>
#include <vector>

namespace th16::sdl {

class Application {
public:
    GraphicsDevice graphics;
    AnmRenderer renderer;
    GameSession session;
    FrameClock clock;
    std::string failure;
    unsigned frames = 0;
    bool initialized = false;

    // 便捷访问（BrowserRuntime 导出用）
    Supervisor& supervisor() { return session.supervisor; }
    TitleScene& title() { return session.title; }

    bool initialize(const char* archive_path);
    // 固定一个逻辑步 + 绘制（测试/步进用）。返回 false = 致命错误。
    bool step();
    // 按墙钟推进（rAF 时间戳秒），可能执行 0..kMaxSteps 个逻辑步，再绘制一帧
    bool step_at(double wall_seconds);

private:
    std::vector<u8> archive_bytes;

    void sample_input();
    void draw_frame();
};

}  // namespace th16::sdl
