// 音频设备（桩实现：静音 + 请求全收；接口先立，后续整件接入
// SDL3 float 流 + miniaudio 混音，ui-base.md §三）。
#pragma once

#include <string>

namespace th13::sdl {

class AudioDevice {
public:
    std::string error;
    i32 music_volume = 100;

    bool initialize(bool device = true) {
        (void)device;
        return true;  // 桩：无音频输出
    }
    bool enqueue(i32) { return true; }  // 音效请求全收（静音）
    bool queue_music(i32, i32, const std::string& = "") { return true; }
    bool finish_music_requests() { return true; }
    void pause_music(bool) {}
    void suspend(bool) {}
    void update() {}
    void pump() {}
    void close() {}
};

}  // namespace th13::sdl
