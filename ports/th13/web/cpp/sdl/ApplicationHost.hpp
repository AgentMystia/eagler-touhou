// SDL3 宿主层：窗口/主循环/时间。骨架阶段仅建立 Emscripten 主循环与帧计数，
// 渲染、音频与输入随子系统还原逐步接入（对齐 ports/th10 的分层约定）。
#pragma once

#include <cstdint>

namespace th13::sdl {

class ApplicationHost {
public:
    int run();  // 网页目标：安装 emscripten 主循环；原生目标：进入本地循环

    uint64_t frameCount() const { return frameCount_; }

private:
    void tick();

    uint64_t frameCount_ = 0;
};

}  // namespace th13::sdl
