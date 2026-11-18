// th16 游戏会话——不依赖 SDL/GL 的逻辑层：档案 + ANM 管理器 + Supervisor + Chain + 标题菜单。
// Application（浏览器）与 tests/harness（WASI/本机）共用同一份时序，保证无头对拍与上屏一致。
//
// 原版对应：0x43ba40 注册主更新链 → 0x43b3d0 每帧泵 → 0x43ce10 场景切换；
// 启动幕 0x43adc0/0x43b300/0x43b290；标题 0x44aee0/0x44abc0/0x44ac70/0x44b5f0。
#pragma once
#include "AnmManager.hpp"
#include "AnmRenderer.hpp"
#include "AnmResource.hpp"
#include "Archive.hpp"
#include "Chain.hpp"
#include "Effect.hpp"
#include "ScreenEffect.hpp"
#include "Spellcard.hpp"
#include "Input.hpp"
#include "Supervisor.hpp"
#include "GameGlobals.hpp"
#include "GameManager.hpp"
#include "TitleScene.hpp"

#include <functional>
#include <string>
#include <vector>

namespace th16 {

class GameSession {
public:
    // 资源装入后的回调（浏览器端建纹理；无头端空）
    using OnResourceLoaded = std::function<bool(AnmResource&)>;

    AnmManager animations;
    EffectManager effects;        // DAT_004a6db8（0x418a30：启动期创建，更新链 0x1f）
    Spellcard spellcard;          // DAT_004a6db0（0x4178a0：进游戏时创建；敌机 / 自机被弹共用其标志）
    ScreenShakeOffset screen_shake;  // DAT_004c1788 / DAT_004c1530：ECL 517 画面震动当前偏移
    std::vector<Spellcard::GuiNotice> gui_events;  // 本帧 0x42bcf0 Gui 通知（Gui 接入前供 harness 对拍）
    Supervisor supervisor;
    ChainManager chains;
    GameGlobals globals;
    TitleScene title;
    GameManager game;              // 场景 7：DAT_004a6dd4
    bool game_live = false;
    bool quit_requested = false;   // 场景 2/3：原版退出进程；宿主据此停循环
    std::vector<TitleScene::Event> title_events;  // 离开标题前的事件归档（harness 断言用）
    std::string failure;
    u32 injected_input = 0;   // 外部注入 held 位（与宿主键盘 OR）
    u32 host_input = 0;       // 宿主键盘采样
    InputState game_input;    // DAT_004a52c8：Replay 记录链（优先级 0x10，0x447fd0）每帧抄 held 低 16 位并做边沿
    // 逻辑侧屏幕几何（DAT_004d9d34 窗口缩放 / DAT_004d9d50,54 场地偏移 / DAT_004d9d48,4c）：自机弹出界判定 0x468c00 用；
    // 原版由 Supervisor 绘制链 0x43b3d0 每帧写为 640/2、(480-448)/2，这里取同值常量
    AnmRenderer geometry;
    std::vector<i32> sfx_events;       // 本帧 0x45e1f0 音效登记（全部管理器；Sound 接入前供 harness 对拍）
    std::vector<i32> player_sfx_events; // 其中由 Player 代码段（0x440000..0x449000：更新/被弹/擦弹）登记的子集
    bool run_draw_chains = true;  // harness 对拍 oracle（不跑 0x4015a0）时关掉
    // harness 提供 enemies 时间表时置 false，由桩保持 PlayerWorld.enemy_count（对齐旧桩语义）
    bool sync_enemy_count = true;
    OnResourceLoaded on_loaded;

    bool open(const u8* archive_bytes, u32 size);
    bool begin_boot();                 // 0x43adc0：ascii + sig，注册启动幕绘制链，请求 Title
    // 跳过启动幕/标题直接进入场景 7（harness）：装启动 bank 后按 globals（stage/shot/season/difficulty）装载关卡
    bool start_game_direct();
    bool logic_step();                 // 一个逻辑帧（输入 → 切换 → 链 → ANM tick）
    const Archive& archive() const { return archive_; }
    AnmResource* resource(const char* name);

private:
    Archive archive_;
    std::vector<AnmResource> resources_;
    std::vector<std::string> resource_names_;
    ChainEntry* boot_draw_chain_ = nullptr;
    ChainEntry* effect_update_chain_ = nullptr;
    ChainEntry* title_update_chain_ = nullptr;
    ChainEntry* game_update_chain_ = nullptr;
    ChainEntry* stage_update_chain_ = nullptr;
    ChainEntry* replay_record_chain_ = nullptr;
    ChainEntry* player_update_chain_ = nullptr;
    ChainEntry* player_draw_chain_ = nullptr;
    ChainEntry* enemy_update_chain_ = nullptr;
    ChainEntry* bullet_update_chain_ = nullptr;
    ChainEntry* bullet_draw_chain_ = nullptr;
    ChainEntry* item_update_chain_ = nullptr;
    ChainEntry* item_draw_chain_ = nullptr;
    ChainEntry* item_draw_season_chain_ = nullptr;
    ChainEntry* spellcard_update_chain_ = nullptr;
    ChainEntry* gui_update_chain_ = nullptr;
    std::vector<ChainEntry*> shake_chains_;  // 0x45d150(1) 实例（链 0x13，自删）

    bool load_anm(const char* name, AnmResource*& out);
    bool load_boot_banks();            // 0x43adc0 / 0x407cb0 / 0x43ad20 / 0x418790 的同步装载部分
    bool create_player();              // 0x42cb60 新游戏段：0x441c60 + 火力/量表初值 + 0x4440e0
    bool enter_title_assets();
    bool activate_title_menu();
    void leave_title();
    bool enter_game();     // 0x43ce10 case 7 → 0x42d700 + 0x42cb60（同步装载）
    void leave_game();     // 0x42d200
    void harvest_player_sfx();
    // 0x45d150(1, …)：登记一条链 0x13 的画面震动（ECL 517 在敌机链内请求，下一帧起消耗 game_rng）
    void create_screen_shake(const EnemyManager::ShakeRequest& request);
    void sample_input();
};

}  // namespace th16
