// 文件存储（桩实现：内存档；成绩/配置/录像持久化延后，接口先立）。
#pragma once

#include <string>

namespace th13::sdl {

class FileStore {
public:
    std::string error;
    bool initialize() { return true; }  // 桩：无落盘
    bool save() { return true; }
};

}  // namespace th13::sdl
