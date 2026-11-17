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
    OnResourceLoaded on_loaded;

    bool open(const u8* archive_bytes, u32 size);
    bool begin_boot();                 // 0x43adc0：ascii + sig，注册启动幕绘制链，请求 Title
    bool logic_step();                 // 一个逻辑帧（输入 → 切换 → 链 → ANM tick）
    const Archive& archive() const { return archive_; }
    AnmResource* resource(const char* name);

private:
    Archive archive_;
    std::vector<AnmResource> resources_;
    std::vector<std::string> resource_names_;
    ChainEntry* boot_draw_chain_ = nullptr;
    ChainEntry* title_update_chain_ = nullptr;
    ChainEntry* game_update_chain_ = nullptr;
    ChainEntry* stage_update_chain_ = nullptr;

    bool load_anm(const char* name, AnmResource*& out);
    bool enter_title_assets();
    bool activate_title_menu();
    void leave_title();
    bool enter_game();     // 0x43ce10 case 7 → 0x42d700 + 0x42cb60（同步装载）
    void leave_game();     // 0x42d200
    void sample_input();
};

}  // namespace th16
