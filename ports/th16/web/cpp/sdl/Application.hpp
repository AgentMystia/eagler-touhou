// th16 应用：SDL/GLES 宿主——打开 th16.dat，把 GraphicsDevice 挂为 GameSession 的 RenderDevice，
// 每个逻辑帧由绘制链（0x45a8a0 帧序）直接向设备出图，最后 Present。
#pragma once
#include "AudioDevice.hpp"
#include "FontPainter.hpp"
#include "GraphicsDevice.hpp"
#include "../game/FrameClock.hpp"
#include "../game/GameSession.hpp"

#include <string>
#include <vector>

namespace th16::sdl {

class Application {
public:
    GraphicsDevice graphics;
    AudioDevice audio;
    FontPainter fonts;
    GameSession session;
    FrameClock clock;
    std::string failure;
    unsigned frames = 0;
    bool initialized = false;

    // 便捷访问（BrowserRuntime 导出用）
    Supervisor& supervisor() { return session.supervisor; }
    TitleScene& title() { return session.title; }

    // 打开 th16.dat / 建图形与音频设备 / 装载启动幕。无音频设备（无 SDL 或打不开）不致命：仅静音
    bool initialize(const char* archive_path);
    // 固定一个逻辑步（含绘制链）+ Present（测试/步进用）。返回 false = 致命错误。
    bool step();
    // 按墙钟推进（rAF 时间戳秒），可能执行 0..kMaxSteps 个逻辑步（每步都跑绘制链——0x409f90 等链带逻辑），
    // 只 Present 最后一帧
    bool step_at(double wall_seconds);

private:
    std::vector<u8> archive_bytes;

    void sample_input();
};

}  // namespace th16::sdl
