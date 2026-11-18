// th16 GameManager——原版 DAT_004a6dd4（0xb4 字节，0x42d700 创建；加载线程 0x42cb60；更新 0x42d7b0）。
//
// 锚点：
//   0x42d700  new 0xb4、flags |= 4（装载中）、+0xac = 模式参数、起线程 0x42d1f0 → 0x42cb60
//   0x42cb60  装载：等启动画面；按 DAT_004a6f18（关卡表）建 Gui 0x447e20 / Stage 0x409db0 / Player 0x427ca0 /
//             Bullet 0x411dd0 / 0x42f440 / Item 0x4314a0 / 0x43e5a0 / 0x449e50 / Enemy(ECL) 0x41b340 /
//             Bomb 0x40d890 / 0x4178a0；BGM 0x43c370；注册更新链 0xf（0x42d7b0）与绘制链 2；清 flags&4
//   0x42d7b0  更新：+0x10（帧计时）== 0 → 0x42dc50 首帧初始化（0x40add0 建面 VM、启用各管理器链、开 BGM）；
//             == 0x1e → 0x42dee0；+0x88 位：4 装载中 / 8 装载失败 / 0x10|0x20|0x40 结束 / 0x800 重试 /
//             0x4000 过关 / 0x10000 关卡切换；末尾 DAT_004a579c++、DAT_004a57a0++、+0xc 计时推进
//
// 本阶段只接入 StageManager（关卡背景）；其它管理器按 roadmap 逐个接入。
#pragma once
#include "AnmVm.hpp"
#include "Bullet.hpp"
#include "Enemy.hpp"
#include "Player.hpp"
#include "Stage.hpp"
#include "StageTable.hpp"
#include "Types.hpp"
#include <memory>

namespace th16 {

class AnmManager;
struct GameGlobals;

class GameManager {
public:
    enum : u32 {
        kLoading = 0x4,
        kLoadFailed = 0x8,
        kEndMask = 0x70,
        kFirstUpdate = 0x80,
        kRetry = 0x800,
        kStageClear = 0x4000,
        kStageTransition = 0x10000,
    };

    u32 flags = kLoading;        // +0x88
    i32 mode = 0;                // +0xac（0x42d700 参数）
    Timer timer;                 // +0x0c
    i32 transition_frame = 0;    // +0x8c
    i32 transition_length = 0;   // +0x90
    i32 end_counter = 0;         // +0xb0
    const StageEntry* entry = nullptr;  // DAT_004a6f18
    StageManager stage;          // DAT_004a6d9c
    bool stage_live = false;
    Player player;               // DAT_004a6ef8（新游戏时 0x441c60 创建）
    bool player_live = false;
    EnemyManager enemies;        // DAT_004a6dc0
    bool enemies_live = false;
    std::unique_ptr<BulletManager> bullets;  // DAT_004a6dac（0x1403b28 字节，堆分配）
    bool bullets_live = false;
    PlayerWorld world;           // 敌人 / 对话 / 过关状态；enemy_count 由 EnemyManager 同步

    // 0x42dc50 首帧初始化（关卡背景 + 生成 "main"）
    void first_frame(AnmManager& manager, GameGlobals& globals);
    // 0x42d7b0：返回 false 表示链应中断（原版返回 3）
    bool update(AnmManager& manager, GameGlobals& globals, float game_speed);
};

}  // namespace th16
