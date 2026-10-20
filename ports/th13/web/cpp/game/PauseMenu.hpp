// 暂停菜单（th15 PauseMenu 裁剪：继续/重开/回标题 三项；Game Over 幕禁用继续；
// 成绩登录与录像保存延后）。菜单延迟与光标历史属于游戏态，
// ANM 树归标题/HUD 共用的可移植管理器，平台工作显式化。
#pragma once

#include "PauseState.hpp"
#include "AnmManager.hpp"

namespace th13 {

// 暂停菜单脚本号（th15 用 front 149–152；th13 front.anm 对应脚本待 Ghidra
// supervisor 锚定后回填，当前值仅保证存在性，ui-base.md §八）。
constexpr i32 kPauseMenuScript = 0;

struct PauseMenuServices {
    virtual ~PauseMenuServices() = default;
    virtual bool sound(i32) = 0;
    // 入口动作：0=继续、1=重开（destination 13）、2=回标题（destination 4）。
    virtual bool finish_selection(i32 entry) = 0;
};

class PauseMenu {
    PauseState& state;
    AnmManager& animations;
    PauseMenuServices& host;
    bool check(bool, const char*);
    bool interrupt(u32, i32, bool immediate = false);
    bool child(i32, i32);
    bool navigate(u32, u32, i32);

public:
    std::string error;
    PauseMenu(PauseState& s, AnmManager& a, PauseMenuServices& h) : state(s), animations(a), host(h) {}
    bool update(u32 pressed, u32 repeated);
};

}  // namespace th13
