// 难度与角色菜单（th13 脚本号重锚，蓝图 §四 + oracle 实测章）。
// 难度根 script1 (70,312)：5 名行（家内安全/無病息災/交通安全/學業成就/やめる，
// 灰 23-27 白 28-32）+ 数字行 33-48（label 与名行同号联动）；大行条 select01
// 101-104+109（行位 (32,288+48k)，golden 难度帧行距拟合，归属存疑待 oracle）。
// 角色根 script2 (70,312)：7 名行（0=EXTRA 字条/1-4=霊夢・魔理沙・早苗・妖夢/
// 5=LUNATIC 字条/6=やめる，位图辨认待复核）+ 数字行 63-82；立绘+武器+选择框
// 挂根 129（EX 分支 130，分支条件待锚定）。
#pragma once

#include "TitleAnimations.hpp"
#include "TitleMainMenu.hpp"
#include "SessionState.hpp"

#include <functional>

namespace th13 {

struct TitleMenuServices {
    virtual ~TitleMenuServices() = default;
    virtual bool sound(i32) = 0;
    virtual bool begin_transition(u32& animation) = 0;
    virtual bool start_game(i32 stage) = 0;
};

class TitleDifficultyMenu {
    TitleState& state;
    SessionState& progress;
    TitleSelectionSettings& settings;
    TitleAnimations visuals;
    TitleSceneResources resources;
    bool check(bool);
    bool sound(i32);
    bool build_rows();           // 大行条+立绘（推定归属，蓝图 §1.7 注记）
    bool retire_rows();          // 行条滑出销毁（label 1）
    void choose_character() noexcept;
    void return_main();

public:
    std::function<bool(i32)> play_sound;
    std::string error;
    TitleDifficultyMenu(TitleState& s, SessionState& p, TitleSelectionSettings& settings,
                        AnmManager& a, TitleSceneResources resources)
        : state(s), progress(p), settings(settings), visuals(s, a, resources.title, resources.ascii),
          resources(resources) {}
    bool update(u32 pressed, u32 repeated);
};

class TitleCharacterMenu {
    TitleState& state;
    SessionState& progress;
    TitleSelectionSettings& settings;
    AnmManager& animations;
    TitleAnimations visuals;
    TitleSceneResources resources;
    TitleMenuServices& host;
    bool check(bool, const char*);
    bool visual(bool);
    bool enter_portrait(i32 character, i32 label);  // 129 子树立绘/武器联动
    void return_difficulty();

public:
    std::string error;
    TitleCharacterMenu(TitleState& s, SessionState& p, TitleSelectionSettings& settings,
                       AnmManager& a, TitleMenuServices& host, TitleSceneResources resources)
        : state(s), progress(p), settings(settings), animations(a),
          visuals(s, a, resources.title, resources.ascii), resources(resources), host(host) {}
    bool update(u32 pressed, u32 repeated);
};

// 角色菜单行位（root2 子脚本 49-55，推定：1-4 为可选角色，0/5 为 EX/LUNATIC
// 字条位图（用途待锚定），6 为やめる）。
constexpr i32 kTitleCharacterRows = 7;
constexpr i32 kTitleCharacterFirst = 1;   // 霊夢行
constexpr i32 kTitleCharacterLast = 4;    // 妖夢行
inline i32 title_character_at(i32 row) { return row - kTitleCharacterFirst; }

}  // namespace th13
