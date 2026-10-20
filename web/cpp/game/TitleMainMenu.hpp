// 标题主菜单（th15 TitleMainMenu 结构移植；th13 菜单项裁剪为本轮链路：
// 开始游戏 →（难度 → 角色）→ StartGame，其余入口禁用待分期，ui-base.md §三）。
// 脚本号登记：主菜单根=title.anm script0（dump：20 子脚本，位置 (70,280)），
// 按键提示=script3（标签 7/17 闪烁、2 确认白闪、3 收场）。th13 supervisor
// 锚定后如与原版不符在此回填。
#pragma once

#include "TitleState.hpp"
#include "SessionState.hpp"
#include "AnmManager.hpp"

#include <functional>

namespace th13 {

// th13 主菜单条目（顺序为原版菜单序，Replay/PlayerData/MusicRoom 本轮禁用）。
enum class TitleMainEntry : i32 { StartGame = 0, Replay = 1, PlayerData = 2, MusicRoom = 3, Quit = 4 };
constexpr i32 kTitleMainEntries = 5;

// 标题脚本表（bank 号已验证：title=16、title_v=17、ascii=2；脚本号证据见文件头）。
struct TitleSceneResources {
    i32 title = 16, portrait = 17, ascii = 2;
    i32 main_root = 0;     // 主菜单根
    i32 main_prompt = 3;   // 「按任意键」提示
    i32 difficulty_root = 1;  // 难度菜单根（script1，26 子脚本，位置 (70,312)；待锚定）
    i32 character_root = 2;   // 角色菜单根（script2，34 子脚本；待锚定）
};

class TitleMainMenu {
    TitleState& state;
    SessionState& progress;
    TitleSelectionSettings& settings;
    AnmManager& animations;
    TitleSceneResources resources;
    bool fail(const std::string&);
    bool create(i32);
    bool present(u32&);
    bool interrupt(i32, i32, bool);
    bool child_interrupt(i32, i32, bool);
    bool paint();
    bool select_animation();
    bool sound(i32);
    void enter(i32 entry);

public:
    std::function<bool(i32)> play_sound;
    std::string error;
    TitleMainMenu(TitleState& state, SessionState& progress, TitleSelectionSettings& settings,
                  AnmManager& animations, TitleSceneResources resources)
        : state(state), progress(progress), settings(settings), animations(animations),
          resources(resources) {}
    bool update(u32 pressed, u32 repeated);
};

}  // namespace th13
