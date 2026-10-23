// 对局战斗核心（回放对拍 + 可玩化）：自机模型 + StageSim 交互步进 + 碰撞/结算。
// ECL/敌机/弹幕推进全部经 StageSim（oracle 契约路径不变，交互模式见
// StageSim::begin）；本模块做「自机/自机弹/碰撞/奖惩」。
//
// 数值口径（event-contract.md §7.2，与 oracle python 孪生同式）：
//   移动 = 原版 0x442960 定点模型——pos(1/128) += speed128 × dir(单位向量表
//   0x4abc30，斜向不归一)，speed128 四选一（focus × 斜向），出自 pl02a.sht
//   （+0x10..+0x1c 浮点 ×128 取整：4.5/2.0/3.1819805/1.4142135 →
//   576/256/407/181），钳制 x∈[-23552,23552]、y∈[4096,55296]，初态 (0,51200)。
//   自机弹出 .sht 弹型域（0x446000/0x445c40 原版语义：帧%间隔==相位出膛、
//   伤害=记录+0x18、块=功率/100+聚焦×5；回放口径功率恒初值 100）。判定半径
//   为定标值【待校准】，AABB 判定对齐 0x446870。全部整数/整值浮点运算，
//   两侧无舍入差（与 oracle/replay_model.py 孪生逐字段同式）。
#pragma once

#include "GameInput.hpp"
#include "SessionState.hpp"
#include "StageSim.hpp"

#include <array>

namespace th13 {

struct StageBattleServices {
    virtual ~StageBattleServices() = default;
    virtual bool life_hud(i32 lives, i32 pieces) = 0;
    virtual bool bomb_hud(i32 bombs, i32 pieces) = 0;
    virtual bool request_game_over() = 0;
    virtual bool hud_sound(i32 id) = 0;
    // 敌机死亡回收钩（0x418e00 销毁支路的 ANM 侧）：宿主回收该敌机全部
    // 槽位 VM（enemies_from_ecl 路径句柄归零防泄漏）。默认空实现——
    // 无头契约路径（stage1/replay 事件窗）无 ANM 面。
    virtual void enemy_retire(u32 serial) { (void)serial; }
};

// 自机弹（垂直直进模型弹；x/y 整值浮点精确步进）。
struct PlayerShot {
    float x = 0, y = 0, damage = 0;
    float angle = -1.5707964f;  // 弹向（渲染旋转用；主弹恒 -π/2 直上）
    i32 script = -1;            // plXX.anm 弹型脚本（.sht 记录；-1 = 精灵直贴回退）
    bool alive = false;
};

// .sht 弹型记录（0x443e40 装载解析：+0xc8 指针表 → +0xf4 基准块，记录
// 步长 0x38、首字节 <0 终止）。字段序 = 文件布局（0x446000/0x445c40 直读）：
// +0 u8 间隔（帧 % 间隔 == 相位才出膛，0x446000:24；pl00a 主弹=3、卫星=8、
// 该字节兼类别）、+1 u8 相位、+2 u16 脚本、+4/+8 f32 {x,y} 出膛偏移
// （anchor≠0 时相对卫星——孪生口径≈自机位，卫星模型分期）、+0x14 f32 角度、
// +0x18 f32 伤害（0x445c40:55，pl00a 主弹=24/卫星=4）、+0x20 i8 卫星锚位。
struct ShtShotRecord {
    i32 interval = 0;  // 发射间隔（= 首字节，兼类别：3 主弹 / 8 卫星弹）
    i32 phase = 0;     // 发射相位（帧 % interval == phase 出膛）
    i32 script = 0;    // plXX.anm 脚本号
    i32 anchor = 0;    // 卫星锚位（0 = 自机；≠0 = Option 卫星，分期）
    float x = 0, y = 0;
    float angle = 0;
    float damage = 0;
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

    // 定标常量（.sht 弹型域以外的域：碰撞/无敌等，§7.2【待校准】）。
    // kFireInterval/kShotDamage/kShotOffset* 仅无 .sht 回退路径使用；
    // 弹型/伤害/间隔/相位全部出自 .sht 记录（0x446000/0x445c40 原版语义）。
    static constexpr i32 kFireInterval = 4;      // 回退：发射间隔帧（相位制）
    static constexpr float kShotDamage = 12.0f;  // 回退：单发伤害（整值）
    static constexpr float kShotSpeed = 12.0f;   // 垂直弹速（整值；sht 速度域
                                                 // +0xc/+0x10=48/16 的运动函数
                                                 // 表语义待 oracle 实证，暂不替换）
    static constexpr float kShotHalf = 4.0f;     // 弹体半宽高
    static constexpr float kShotOffsetX = 8.0f;  // 无 .sht 回退：三发 x 偏移
    static constexpr float kShotOffsetY = 12.0f; // 无 .sht 回退：出膛 y 偏移
    static constexpr float kPlayerHitRadius = 3.0f;
    static constexpr float kBulletHitRadius = 4.0f;
    static constexpr float kEnemyBodyRadius = 20.0f;
    static constexpr i32 kInvulnFrames = 240;
    static constexpr i32 kBombFrames = 180;
    static constexpr float kBombDamagePerFrame = 3.0f;
    static constexpr i32 kKillScore = 5000;
    static constexpr i32 kKillPower = 8;

    // sht_name：移速 + 弹型来源（回放对拍 = plXXa/b.sht；缺文件时失败）。
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

    // .sht 弹型域：5 能量档非聚焦（块 0-4）+ 5 能量档聚焦（块 5-9）+
    // 特殊块 10（类别 2，未接入）。
    static constexpr u32 kShtBlocks = 11;
    std::array<std::vector<ShtShotRecord>, kShtBlocks> sht_shots;
    bool sht_shots_loaded = false;

private:
    void move_player(u32 held);
    void fire_shots(i32 frame);
    void step_shots();
    void collide_shots();
    void collide_player();
    void reward_kill(StageEnemy& enemy);
    bool kill_player();
    StageBattleServices* host_ = nullptr;
    SessionState* progress_ = nullptr;
    HudPlayerState* player_ = nullptr;
    HudScoreState* score_ = nullptr;
    i32 fire_frame_ = 0;  // 发射相位帧计数（1 基，自对局首帧；0x446000 的
                          // 帧 % 间隔 == 相位判决源，玩家 ctx+0x146cc 同源口径）
    bool life_probe_ = false;  // TH13_LIFE_PROBE=1：敌机生命轨迹对拍探针
    bool events_ = false;  // 事件落盘（回放对拍）
    std::string error_;
};

}  // namespace th13
