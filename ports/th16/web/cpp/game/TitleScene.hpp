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
//   2 = 标题结束（0x44af80 case 2 置场景请求）；3 Option 0x44c570；4 Key Config（自 Option）；
//   0xa Player Data / 0xb Replay / 0xd Music Room / 0x10 Manual / 0x11 Spell Practice 待还原。
#pragma once
#include "AnmManager.hpp"
#include "GameGlobals.hpp"
#include "Input.hpp"
#include "MenuCursor.hpp"
#include "Types.hpp"

#include <functional>
#include <string>
#include <vector>

namespace th16 {

struct HelpScreen;  // GameMenu.hpp：标题 Manual / 暂停 Manual 共用

class TitleScene {
public:
    enum Screen : i32 {
        kNone = 0, kMainMenu = 1, kFinished = 2, kOption = 3, kKeyConfig = 4, kDifficulty = 5, kShot = 6,
        kSeason = 7, kStage = 8, kScore = 0xa, kReplay = 0xb, kMusicRoom = 0xd, kManual = 0x10,
        kSpellPractice = 0x11, kSpellList = 0x12, kSpellShot = 0x13,
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
    // 标题 Replay 画面 0x451750
    MenuCursor page_cursor;          // +0x1d4：页（0..2），槽 = page*0x19 + cursor
    bool replay_slot_live[100] = {}; // +0x5b50：槽是否有文件
    i32 replay_stage_mask[100] = {}; // 每槽已有关卡位图（bit0=stage1…）；确认后用于禁用项
    bool replay_spell_practice[100] = {};
    i32 replay_choice = -1;          // +0x5b48：选中的绝对槽
    i32 replay_stage_choice = 0;     // +0x5b4c：开局关卡下标（1-based 存到 DAT_004a5790）
    u32 replay_ui_flags = 0;         // +0x5ce8：bit2 离开中 / bit3 扫描完成
    i32 last_replay_slot = 0;        // DAT_004a6f28：再次进入时恢复
    std::string replay_directory;    // 宿主：th16_XX.rpy 所在目录
    std::string pending_replay_path; // 确认播放：GameSession case 0xd 装载
    HelpScreen* help = nullptr;      // DAT_004a6dd8：标题 Manual 0x4545a0
    std::vector<std::string> music_tracks;  // musiccmt.txt 的 @bgm/... 路径
    i32 music_scroll = 0;            // +0x5a44 窗口首项（可见约 10 条）
    MenuCursor spell_shot_cursor;    // +0x5cec：符卡练习机体副光标
    i32 spell_group = 0;             // +0x5dc4：选中的分组（确认后带入列表）
    MenuCursor score_shot_cursor;    // +0xfc：Player Data 机体（count 5）
    MenuCursor score_page_cursor;    // +0x1d4：成绩翻页
    GameGlobals* globals = nullptr;
    std::vector<Event> events;       // SFX / 画面 / 场景请求 / 过场效果（供 harness 断言）
    // GameSession 注入：0x45d150 淡入淡出 / 0x41a390 Loading + 0x418ba0 过场
    std::function<void(i32 kind, i32 duration, u32 color)> on_screen_fade;
    std::function<void()> on_transition_effect;
    std::function<AnmFile*()> load_help_anm;  // 0x42e760：help.anm

    void activate(AnmFile* title, AnmFile* title_v, AnmFile* ascii, GameGlobals& g) {
        close_help();
        auto fade = on_screen_fade;
        auto transition = on_transition_effect;
        auto help_loader = load_help_anm;
        auto replay_dir = replay_directory;
        *this = TitleScene{};
        title_file = title;
        title_v_file = title_v;
        ascii_file = ascii;
        globals = &g;
        screen = kNone;
        on_screen_fade = std::move(fade);
        on_transition_effect = std::move(transition);
        load_help_anm = std::move(help_loader);
        replay_directory = std::move(replay_dir);
    }
    bool live() const { return title_file != nullptr; }

    // 0x44af80：每帧由标题更新链（优先级 6）调用
    void update(AnmManager& anm, const InputState& input);
    // 主菜单空闲 demo 门控（0x708 / bit6 / 下标 %3）；供单测与 update 共用
    void tick_idle_demo(const InputState& input);
    void close_help();  // 释放标题 Manual 的 HelpScreen
    void parse_music_cmt(const char* text, size_t len);  // musiccmt.txt → music_tracks

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
    void update_option(AnmManager& anm, const InputState& input);        // 0x44c570
    void update_replay(AnmManager& anm, const InputState& input);        // 0x451750
    void update_manual(AnmManager& anm, const InputState& input);        // 0x4545a0
    void update_music(AnmManager& anm, const InputState& input);         // 0x4546f0
    void update_spell_practice(AnmManager& anm, const InputState& input); // 0x4553d0
    void update_spell_list(AnmManager& anm, const InputState& input);    // 0x455d50
    void update_score(AnmManager& anm, const InputState& input);         // 0x452330 Player Data
    void update_key_config(AnmManager& anm, const InputState& input);    // 0x44e930
    void update_stub(AnmManager& anm, const InputState& input);          // 待还原画面：取消回上层
    void request_game(i32 stage, i32 table);                             // DAT_004c17c4 = 7 等
    void transition_effect();                                            // 0x41a390 + 0x418ba0
    void refresh_option_items(AnmManager& anm);                          // 0x44c8c0：面板子项 0x1e/0x1f
    void apply_option_volume(AnmManager& anm);                           // 0x44dc70：音量数字 ANM（缺 VM 则跳过）
    void panel_item_interrupt_now(AnmManager& anm, i32 script, i32 value);
    void scan_replay_slots();                                            // 同步替代 0x402fb0 扫描线程
    i32 replay_abs_slot() const { return page_cursor.index * 0x19 + cursor.index; }
};

}  // namespace th16
