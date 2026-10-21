// 对局战斗核心（Stage1 可玩化）：玩家操控 + StageSim 交互步进 + 碰撞/结算。
// ECL/敌机/弹幕推进全部经 StageSim（oracle 契约路径不变，交互模式见
// StageSim::begin）；本模块只做原版「自机/自机弹/碰撞/奖惩」的可玩子集。
//
// 数值口径：ECL 侧（敌机生命/弹速/角度）为原版数据；自机侧常量是可玩化
// 近似——.sht 弹型表与原版碰撞半径表未接入（分期），文件内以「近似」标注。
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

// 自机弹（直进；弹型表接入前不做追踪/贯通）。
struct PlayerShot {
    float x = 0, y = 0, angle = 0, speed = 0, damage = 0;
    bool alive = false;
};

class StageBattle {
public:
    StageSim sim;
    // 自机状态（game 坐标：x∈[-192,192] y∈[0,448]）
    float player_x = 0.0f, player_y = 416.0f;
    bool focus = false, player_dead = false;
    i32 invulnerable = 0;   // 无敌剩余帧（出生/中弹/炸弹）
    i32 respawn_timer = 0;  // 中弹后的复活等待（近似：原地即刻重生 + 无敌）
    i32 bomb_timer = 0;     // 炸弹作用剩余帧
    std::vector<PlayerShot> shots;

    // 近似常量（可玩化定标，见文件头）
    static constexpr float kPlayerSpeed = 4.0f;      // 未聚焦移速（Reimu 档）
    static constexpr float kPlayerSpeedFocus = 2.0f; // 聚焦移速
    static constexpr float kPlayerHitRadius = 3.0f;  // 判定半径
    static constexpr float kBulletHitRadius = 4.0f;  // 敌弹判定半径（统一近似）
    static constexpr float kEnemyBodyRadius = 20.0f; // 敌机本体碰撞半径近似
    static constexpr i32 kFireInterval = 4;          // 射击间隔帧
    static constexpr float kShotDamage = 12.0f;      // 单发伤害
    static constexpr float kShotSpeed = 12.0f;
    static constexpr i32 kInvulnFrames = 240;        // 中弹/出生无敌
    static constexpr i32 kBombFrames = 180;          // 炸弹持续
    static constexpr float kBombDamagePerFrame = 3.0f;
    static constexpr i32 kKillScore = 5000;          // 击破分（掉落/连锁未接入）
    static constexpr i32 kKillPower = 8;             // 击破灵力（power 单位 0.01）

    bool load(EclResourceProvider& provider, const char* stage_file, i32 difficulty_rank,
              StageBattleServices& host, SessionState& progress, HudPlayerState& player,
              HudScoreState& score);
    // 一帧逻辑：输入 → 自机/自机弹 → ECL 步进 → 碰撞 → 奖惩/HUD。
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
    std::string error_;
};

}  // namespace th13
