// 标题主菜单（th13 真机流程重写，oracle 标题窗实证见 docs/plans/title-anm-blueprint.md
// §oracle 实测章）：boot 无「按任意键」提示幕，开场 sig.anm logo 幕 180 帧后
// 标题展开，菜单 intro 180 帧插值、g361 建菜单、g372 起可交互（全局帧锚点，
// oracle/title_oracle.py docstring）。
// 主菜单 10 项（root script0 @ (70,280)，子 3-22：灰 3-12 层 26 / 白 13-22 层 25）；
// 字条内容按 title01.png 位图辨认：開始/Extra開始/練習開始/スペル練習/リプレイ/
// プレイヤーデータ/音楽室/オプション/マニュアル/やめる（人类辨认待复核）。
// 1 号项 Extra Start 无存档数据被禁用（真机 scoreth13.dat 缺省 → 光标跳过、
// 字条黑化 alpha160，oracle 顶点色 0xA0000000 实证）。
#pragma once

#include "TitleState.hpp"
#include "SessionState.hpp"
#include "AnmManager.hpp"

#include <functional>

namespace th13 {

// th13 主菜单条目（顺序 = title01.png 字条行序，位图辨认；语义标注见上）。
// 本轮链路仅实现 StartGame(0) → 难度 → 角色 → 开局；Quit(9) 出退出请求；
// 其余入口 enter() 播确认音后不切屏（真机有画面，蓝图「未实现」清单登记）。
enum class TitleMainEntry : i32 {
    StartGame = 0,
    ExtraStart = 1,   // 无存档禁用（oracle：↓ 一次 0→2 跳过）
    PracticeStart = 2,
    SpellPractice = 3,
    Replay = 4,
    PlayerData = 5,
    MusicRoom = 6,
    Option = 7,
    Manual = 8,
    Quit = 9,
};
constexpr i32 kTitleMainEntries = 10;

// 真机 boot 时序锚（oracle 标题窗，全局帧；标题任务使能 = g181 为 age 0）：
constexpr i32 kTitleBootIntroFrames = 180;  // g181→g361 菜单 intro 插值
constexpr i32 kTitleMenuReadyDelay = 11;    // g361 建菜单 → g372 可交互

// 标题脚本表（脚本号 = artifacts/reference/dumps/anm-list.txt title.anm 段，
// 蓝图 §一；层/锚点/标签语义见蓝图各节。bank 表 Application.cpp kBanks）。
struct TitleSceneResources {
    i32 title = 16;    // title.anm（23 entry / 242 脚本）
    i32 version = 17;  // title_v.anm：仅版本条 script0（bank17 无立绘，蓝图 §1.9）
    i32 ascii = 2;     // ascii.anm（boot 无提示幕，本轮不建 ascii VM）
    i32 help = 19;     // help.anm（析构 retire_resource 用，蓝图 §五-12）
    i32 main_root = 0;            // 主菜单根 (70,280)，spawn 3-22
    i32 difficulty_root = 1;      // 难度根 (70,312)，spawn 23-48（5 名行+数字行）
    i32 character_root = 2;       // 角色根 (70,312)，spawn 49-82（7 名行+数字行）
    i32 char_menu_root = 129;     // 角色菜单根（立绘 125-128+武器 131-138+选择框）
    i32 char_menu_root_ex = 130;  // EX 武器分支根（129/130 分支条件待锚定，蓝图 §1.8）
    i32 background_root = 89;     // 背景根（spawn 92,93,91,90；内建 110f/170f 时序）
    i32 logo_root = 94;           // LOGO 首入场根（110f 延迟翻转，oracle g291 实证）
    i32 logo_restore_root = 95;   // LOGO 复位根（自子菜单/对局返回，时机待 oracle）
    i32 version_bar = 0;          // title_v script0 @(256,224) 层 26（30f 延迟淡入）
    i32 portrait_base = 125;      // 立绘 125+角色号（label 7/12/17/22+角色号）
    i32 weapon_base = 135;        // 通常武器图标 135+角色号（同上标签族）
    i32 weapon_base_ex = 139;     // EX 武器图标 139+角色号
    i32 weapon_selector_base = 131;  // 武器选择框 131+k（用途待锚定）
    // 难度/角色大行条（select01.png 256×48，蓝图 §1.7；行位 (32,288+48k) 由
    // golden 难度帧行距 48px 拟合，屏幕归属蓝图标注存疑待 oracle 窗口比对）：
    i32 rank_row_base = 101;      // 家内安全(EASY)…學業成就(LUNATIC) 大行条 101-104
    i32 rank_row_quit = 109;      // 「やめる」大行条（select01 第 10 条，位图辨认）
    i32 rank_row_extra = 105;     // EXTRA 大行条（Extra 解锁时启用？待锚定）
};

class TitleMainMenu {
    TitleState& state;
    SessionState& progress;
    TitleSelectionSettings& settings;
    AnmManager& animations;
    TitleSceneResources resources;
    bool fail(const std::string&);
    bool create(i32);
    bool create_at(i32, const Vec3& translation);
    bool present(u32&);
    bool interrupt(i32, i32, bool);
    bool child_interrupt(i32, i32, bool);
    bool sound(i32);
    void adopt_background_layers();  // [窗A] 背景半张层值按 oracle 实测（§8.3）
    bool open_menu();     // g361 菜单展开（root0+版本条+全项入场+禁用黑化）
    bool select_entry();  // interrupt(0,7+cursor) 选中入场
    bool show_menu();     // 全项入场（真机 boot 全员可见，oracle 顶点色实证）
    void dim_disabled();  // 禁用项黑化 alpha160（oracle 0xA0000000）
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
