// 难度与角色菜单（th15 TitleSelectionMenu/TitleCharacterMenu 裁剪移植）。
// th13：难度 Easy/Normal/Hard/Lunatic（Extra 需通关节件，本轮禁用）；
// 角色 pl00–pl03（Reimu/Marisa/Sanae/妖梦，选人即定 bank，ui-base.md §七）。
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
    bool selected_children(i32 root);
    void return_difficulty();

public:
    std::string error;
    TitleCharacterMenu(TitleState& s, SessionState& p, TitleSelectionSettings& settings,
                       AnmManager& a, TitleMenuServices& host, TitleSceneResources resources)
        : state(s), progress(p), settings(settings), animations(a),
          visuals(s, a, resources.title, resources.ascii), resources(resources), host(host) {}
    bool update(u32 pressed, u32 repeated);
};

}  // namespace th13
