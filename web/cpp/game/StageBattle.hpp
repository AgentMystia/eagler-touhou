// 对局战斗核心（回放对拍 + 可玩化）：自机模型 + StageSim 交互步进 + 碰撞/结算。
// ECL/敌机/弹幕推进全部经 StageSim（oracle 契约路径不变，交互模式见
// StageSim::begin）；本模块做「自机/自机弹/碰撞/奖惩」。
//
// 数值口径（event-contract.md §7.2，与 oracle python 孪生同式）：
//   移动 = 原版 0x442960 定点模型——pos(1/128) += speed128 × dir(单位向量表
//   0x4abc30，斜向不归一)，speed128 四选一（focus × 斜向），出自 pl02a.sht
//   （+0x10..+0x1c 浮点 ×128 取整：4.5/2.0/3.1819805/1.4142135 →
//   576/256/407/181），钳制 x∈[-23552,23552]、y∈[4096,55296]，初态 (0,51200)。
//   自机弹/判定半径为定标值（.sht 弹型域未接入）【待校准】，AABB 判定
//   对齐 0x446870。全部整数/整值浮点运算，两侧无舍入差。
#pragma once

#include "GameInput.hpp"
#include "SessionState.hpp"
#include "StageSim.hpp"

namespace th13 {

struct StageBattleServices {
    virtual ~StageBattleServices() = default;
    virtual bool life_hud(i32 lives, i32 pieces) = 0;
    virtual bool bomb_hud(i32 bombs, i32 pieces) = 0;
    virtual bool request_game_over() = 0;
    virtual bool hud_sound(i32 id) = 0;
};

// 自机弹（垂直直进模型弹；x/y 整值浮点精确步进）。
struct PlayerShot {
    float x = 0, y = 0, damage = 0;
    bool alive = false;
};

class StageBattle {
public:
    StageSim sim;
    // 自机对外浮点位置（= 定点 × 0.0078125，无损）；game 坐标 x∈[-192,192]。
    float player_x = 0.0f, player_y = 400.0f;
    bool focus = false, player_dead = false;
    i32 invulnerable = 0;   // 无敌剩余帧（出生/中弹/炸弹）
    i32 bomb_timer = 0;     // 炸弹作用剩余帧
    std::vector<PlayerShot> shots;

    // 定点模型状态（§7.2）。
    i32 pos_fx = 0, pos_fy = 51200;             // 1/128 定点，初态 (0,400)
    i32 speed_orth = 576, speed_diag = 407;      // 非聚焦（pl02a.sht ×128）
    i32 speed_orth_focus = 256, speed_diag_focus = 181;
    u32 input_state = 0;                         // 当帧回放输入（player 事件用）

    // 定标常量（弹型表接入前的可玩/对拍子集，§7.2【待校准】）。
    static constexpr i32 kFireInterval = 4;      // 射击间隔帧
    static constexpr float kShotDamage = 12.0f;  // 单发伤害（整值）
    static constexpr float kShotSpeed = 12.0f;   // 垂直弹速（整值）
    static constexpr float kShotHalf = 4.0f;     // 弹体半宽高
    static constexpr float kShotOffsetX = 8.0f;  // 三发 x 偏移
    static constexpr float kShotOffsetY = 12.0f; // 出膛 y 偏移
    static constexpr float kPlayerHitRadius = 3.0f;
    static constexpr float kBulletHitRadius = 4.0f;
    static constexpr float kEnemyBodyRadius = 20.0f;
    static constexpr i32 kInvulnFrames = 240;
    static constexpr i32 kBombFrames = 180;
    static constexpr float kBombDamagePerFrame = 3.0f;
    static constexpr i32 kKillScore = 5000;
    static constexpr i32 kKillPower = 8;

    // sht_name：移速来源（回放对拍 = pl02a.sht；缺文件时失败）。
    bool load(EclResourceProvider& provider, const char* stage_file, i32 difficulty_rank,
              StageBattleServices& host, SessionState& progress, HudPlayerState& player,
              HudScoreState& score, const char* event_path = nullptr,
              const char* sht_name = nullptr);
    // 一帧逻辑：输入 → 自机/自机弹 → ECL 步进 → 碰撞 → 奖惩/HUD。
    // 事件落盘开启时：帧号自 1 起（0 = start() 装载趟），player 事件为本帧
    // 首条（§7.3）。
    bool step(const GameInput& input);
    void on_bomb_key();  // X 边沿（经 step 内部处理，这里供测试直调）
    const std::string& error() const { return error_; }

private:
    void move_player(u32 held);
    void fire_shots();
    void step_shots();
    void collide_shots();
    void collide_player();
    void reward_kill(StageEnemy& enemy);
    bool kill_player();
    StageBattleServices* host_ = nullptr;
    SessionState* progress_ = nullptr;
    HudPlayerState* player_ = nullptr;
    HudScoreState* score_ = nullptr;
    i32 fire_cooldown_ = 0;
    bool events_ = false;  // 事件落盘（回放对拍）
    std::string error_;
};

}  // namespace th13
