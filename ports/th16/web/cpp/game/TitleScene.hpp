// th16 标题场景——原版 Title 对象（DAT_004a6f20，0x44aee0 创建；更新 0x44af80 优先级 6）。
//
// 对象偏移：
//   +0x10 title.anm 槽  +0x14 title_v.anm 槽
//   +0x18 画面（0x44a560）  +0x1c 上一画面  +0x20 画面内子状态（0x44a5e0）
//   +0x24 MenuCursor（0xd8 字节）
//   +0x2ac Timer（0x44af80 末尾每帧 +1；0x44a560/0x44a5e0 清零）
//   +0x2c0 VM id 表，按脚本号索引（+0x2c0 + script*4）：0 logo，0x61/0x65 装饰，0x68 难度底，0x69 机体底，
//          0x6a 季节底，0x71 关卡底，0x7e/0x7f 难度面板，0x96/0x98 机体面板，0x97/0x99 季节面板，
//          0x11f（+0x73c）= ascii.anm 0x13（落叶粒子发射器，0x44fe20 难度画面生成、0x44ad20 析构时 interrupt 1），0x14c = title_v script0
//
// 画面（+0x18）：1 主菜单 0x44b5f0；5 难度 0x44fe20；6 机体 0x4502c0；7 子季节 0x450af0；8 关卡 0x450ef0；
//   2 = 标题结束（0x44af80 case 2 置场景请求）；3 Option / 0xa Player Data / 0xb Replay / 0xd Music Room /
//   0x10 Manual / 0x11 Spell Practice 待还原（当前：进入后仅接受取消回主菜单）。
#pragma once
#include "AnmManager.hpp"
#include "GameGlobals.hpp"
#include "Input.hpp"
#include "MenuCursor.hpp"
#include "Types.hpp"

#include <functional>
#include <vector>

namespace th16 {

class TitleScene {
public:
    enum Screen : i32 {
        kNone = 0, kMainMenu = 1, kFinished = 2, kOption = 3, kDifficulty = 5, kShot = 6, kSeason = 7,
        kStage = 8, kScore = 0xa, kReplay = 0xb, kMusicRoom = 0xd, kManual = 0x10, kSpellPractice = 0x11,
    };
    static constexpr i32 kVmTable = 0x150;
    static constexpr i32 kLogo = 0;
    static constexpr i32 kTitleV = 0x14c;
    static constexpr i32 kLeafEmitter = 0x11f;        // +0x73c
    static constexpr i32 kLeafEmitterScript = 0x13;   // ascii.anm：每帧 ins_502 生成脚本 0x12 落叶（mode 8）
    static constexpr i32 kSceneGame = 7;

    struct Event { const char* kind; i32 value; };

    i32 screen = kNone;          // +0x18
    i32 prev_screen = kNone;     // +0x1c
    i32 sub = 0;                 // +0x20
    MenuCursor cursor;           // +0x24
    i32 prev_cursor = 0;         // +0x28
    i32 timer = 0;               // +0x2b0
    u32 vm[kVmTable] = {};       // +0x2c0
    AnmFile* title_file = nullptr;   // +0x10
    AnmFile* title_v_file = nullptr; // +0x14
    AnmFile* ascii_file = nullptr;   // AsciiManager 槽 2
    bool has_clear_data = false;     // 0x44a850()：任一难度有通关记录（Extra 可选）
    i32 idle_frames = 0;             // DAT_004a5bf0：主菜单无输入帧计数
    i32 demo_index = 0;              // DAT_004a5bf4：demo1..3 轮转（%3）
    i32 pending_demo = -1;           // 本帧触发的 demo 下标；GameSession 装文件后清
    GameGlobals* globals = nullptr;
    std::vector<Event> events;       // SFX / 画面 / 场景请求 / 过场效果（供 harness 断言）
    // GameSession 注入：0x45d150 淡入淡出 / 0x41a390 Loading + 0x418ba0 过场
    std::function<void(i32 kind, i32 duration, u32 color)> on_screen_fade;
    std::function<void()> on_transition_effect;

    void activate(AnmFile* title, AnmFile* title_v, AnmFile* ascii, GameGlobals& g) {
        *this = TitleScene{};
        title_file = title;
        title_v_file = title_v;
        ascii_file = ascii;
        globals = &g;
        screen = kNone;
    }
    bool live() const { return title_file != nullptr; }

    // 0x44af80：每帧由标题更新链（优先级 6）调用
    void update(AnmManager& anm, const InputState& input);
    // 主菜单空闲 demo 门控（0x708 / bit6 / 下标 %3）；供单测与 update 共用
    void tick_idle_demo(const InputState& input);

private:
    // 0x44a560 / 0x44a5e0
    void set_screen(i32 s) { prev_screen = screen; screen = s; sub = 0; timer = 0; }
    void set_sub(i32 s) { sub = s; timer = 0; }
    void sfx(i32 id) { events.push_back({"sfx", id}); }

    // 0x44a790：vm[slot] 子树里脚本号 == script 的 VM id（无则 0）
    u32 child_id(AnmManager& anm, i32 slot, i32 script);
    // 0x44a700 / 0x44a680（logo 子项）
    void item_interrupt_now(AnmManager& anm, i32 script, i32 value);
    void item_interrupt(AnmManager& anm, i32 script, i32 value);
    // 0x44a650：interrupt(vm[slot],1) 并清零
    void dismiss(AnmManager& anm, i32 slot);
    // 0x46f340：清 flags_lo bit1（隐藏）并递归子树
    static void reveal(AnmManager& anm, u32 id);
    void spawn_into(AnmManager& anm, i32 slot, AnmFile& file, i32 script);

    void update_main_menu(AnmManager& anm, const InputState& input);     // 0x44b5f0
    void refresh_main_items(AnmManager& anm);
    void update_difficulty(AnmManager& anm, const InputState& input);    // 0x44fe20
    void update_shot(AnmManager& anm, const InputState& input);          // 0x4502c0
    void update_season(AnmManager& anm, const InputState& input);        // 0x450af0
    void update_stage(AnmManager& anm, const InputState& input);         // 0x450ef0
    void update_stub(AnmManager& anm, const InputState& input);          // 待还原画面：取消回主菜单
    void request_game(i32 stage, i32 table);                             // DAT_004c17c4 = 7 等
    void transition_effect();                                            // 0x41a390 + 0x418ba0
};

}  // namespace th16
