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
//   伤害=记录+2、弹速=记录+0x18、动画=记录+0x22 再加 10；
//   块=功率/100+聚焦×5；回放口径功率恒初值 100）。判定半径
//   为定标值【待校准】，AABB 判定对齐 0x446870。全部整数/整值浮点运算，
//   两侧无舍入差（与 oracle/replay_model.py 孪生逐字段同式）。
#pragma once

#include "GameInput.hpp"
#include "ItemManager.hpp"
#include "SessionState.hpp"
#include "StageSim.hpp"
#include "YoumuBomb.hpp"

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
    float speed = 12.0f;       // 无 SHT 的旧回退；生产弹从 SHT +0x18 取值。
    float width = 8.0f, height = 8.0f;
    i32 age = 0, behavior = 0;
    u32 target_serial = 0;
};

// 生产战斗和表现层共用的卫星/梦想封印状态，坐标均为游戏坐标。
struct PlayerOption {
    float x = 0, y = 0, angle = -1.5707964f;
    bool active = false;
};
struct PlayerBombOrb {
    float x = 0, y = 0, angle = 0;
    bool active = false, exploding = false;
    i32 explosion_frame = 0;
    float radius = 0, speed = 0;
    float velocity_x = 0, velocity_y = 0;
    float accumulated_damage = 0;
};

// 命中扫描用的敌机快照（每帧 sim.step_frame 之前采集）：原版 0x419a90 的
// 自机弹扫描（0x446870）发生在敌机 0x419450 积分之前，故 G 帧命中判定用
// G-1 帧末位置/受击尺寸（G 状态流 th13_01 实证：旧序提前一帧命中）。
struct EnemyScanState {
    u32 serial = 0;
    float x = 0, y = 0;
    float hurtbox[2] = {0, 0};
};

// .sht 弹型记录（0x443e40 装载解析：+0xc8 指针表 → +0xf4 基准块，记录
// 步长 0x38、首字节 <0 终止）。字段序 = 文件布局（0x446000/0x445c40 直读）：
// +0 u8 间隔（帧 % 间隔 == 相位才出膛，0x446000:24；pl00a 主弹=3、卫星=8）、
// +1 u8 相位、+2 i16 伤害、+4/+8 f32 {x,y} 出膛偏移（anchor≠0 时相对卫星，
// 卫星模型分期）、+0x14 f32 角度、+0x18 f32 弹速、+0x20 i8 卫星锚位、
// +0x22 i16 动画编号（实际 ANM 脚本 = 编号 + 10，00445e0c–00445e47）。
struct ShtShotRecord {
    i32 interval = 0;  // 发射间隔（= 首字节，兼类别：3 主弹 / 8 卫星弹）
    i32 phase = 0;     // 发射相位（帧 % interval == phase 出膛）
    i32 script = 0;    // plXX.anm 脚本号
    i32 anchor = 0;    // 卫星锚位（0 = 自机；≠0 = Option 卫星，分期）
    float x = 0, y = 0;
    float angle = 0;
    float damage = 0;
    float speed = 0;
    float width = 0, height = 0;
    i32 behavior = 0, type = 0;
};

class StageBattle {
public:
    StageBattle() = default;
    // 共享 game RNG 注入（R 模块 0x4dc324 位接线）：传入时 sim（ECL/弹幕/
    // etEx）与自机弹出膛共用同一实例；缺省 = 自持（tests 无头路径不破）。
    explicit StageBattle(GameRandom* shared_random)
        : sim(shared_random), item_manager(shared_random) {}
    StageSim sim;
    // 道具/神霊管理器（0x42f020/0x438ec0/0x42e380/0x438730）；load 时注入
    // sim.items（StageItemHost），每帧经 update_item_side 步进。
    ItemManager item_manager;
    // 妖梦 Bomb 斩击池（0x40cb50 战斗侧，r4/bomb 模块）：step_bomb 内 step
    // 生成、bomb_damage/clears 消费；set_vm 由表现层（pl03 VM 持有者）在
    // ANM tick 后回填——每帧序 = battle 生成支路（1 次 signed_unit 抽）→
    // VM tick（ins_119 抽）→ set_vm → 命中/消弹查询（原版帧序）。
    YoumuBomb youmu_bomb_;
    // 本帧斩击生成事件（slot/script/y；slot=-1 仅视觉）：step_bomb 产出，
    // 表现层（Items/pl03 VM）读后建 VM；每帧 step 开头清空。
    std::vector<YoumuBomb::Spawn> youmu_slash_spawns_;
    // 自机对外浮点位置（= 定点 × 0.0078125，无损）；game 坐标 x∈[-192,192]。
    float player_x = 0.0f, player_y = 400.0f;
    bool focus = false, player_dead = false;
    // 原版 player+0x65c / +0x668；碰撞不等于已经扣命。
    enum class PlayerPhase : i32 { Entering = 0, Active = 1, Dying = 2, Deathbomb = 4 };
    PlayerPhase player_phase = PlayerPhase::Active;
    i32 player_phase_frame = 0;
    bool player_visible() const noexcept {
        return !player_dead && player_phase != PlayerPhase::Dying;
    }
    i32 invulnerable = 0;   // 无敌剩余帧（出生/中弹/炸弹）
    i32 bomb_timer = 0;     // 炸弹作用剩余帧
    i32 bomb_age = -1;      // 0040a270 ctx+0x1c；未施放=-1
    std::array<PlayerBombOrb, 8> bomb_orbs{};
    std::array<PlayerOption, 8> options{};
    i32 option_count = 0;
    i32 player_character() const noexcept { return character_; }
    i32 player_power() const noexcept { return player_ ? player_->power : 0; }
    i32 player_power_max() const noexcept { return player_ ? player_->max_power : 0; }
    i32 power_level() const noexcept {
        return player_ && player_->power_step > 0 ? player_->power / player_->power_step : 0;
    }
    bool full_power() const noexcept {
        return player_ && player_->power >= player_->max_power;
    }
    std::vector<PlayerShot> shots;

    // 定点模型状态（§7.2）。
    i32 pos_fx = 0, pos_fy = 51200;             // 1/128 定点，初态 (0,400)
    i32 speed_orth = 576, speed_diag = 407;      // 非聚焦（pl02a.sht ×128）
    i32 speed_orth_focus = 256, speed_diag_focus = 181;
    u32 input_state = 0;                         // 当帧回放输入（player 事件用）

    // 定标常量（.sht 弹型域以外的域：碰撞/无敌等，§7.2【待校准】）。
    // kFireInterval/kShotDamage/kShotOffset* 仅无 .sht 回退路径使用；
    // 弹型/伤害/弹速/间隔/相位全部出自 .sht 记录（0x446000/0x445c40）。
    static constexpr i32 kFireInterval = 4;      // 回退：发射间隔帧（相位制）
    static constexpr float kShotDamage = 12.0f;  // 回退：单发伤害（整值）
    static constexpr float kShotSpeed = 12.0f;   // 仅无 SHT 的旧回退；生产用记录 +0x18。
    static constexpr float kShotHalf = 4.0f;     // 弹体半宽高
    static constexpr float kShotOffsetX = 8.0f;  // 无 .sht 回退：三发 x 偏移
    static constexpr float kShotOffsetY = 12.0f; // 无 .sht 回退：出膛 y 偏移
    // 自机判定尺寸（0x441dd0:65 + .data 0x4c0ba8[be7b8]）：DAT_004be7b8 =
    // 选人光标 0..3；常规路径 be7bc=0，故下标 = character。值 = 2/3/3/3，
    // 原版存“全尺寸”：0x444140 弹幕圆形判定以 尺寸² 参与（port collide_player
    // 的 p2 = p*p 同式）；0x441dd0 以 ×0.5 写入玩家矩形半宽 0x620/0x624
    //（0x443fd0 AABB 分支用半宽 = 尺寸/2，collide_player 同式）。
    // 敌机本体碰撞（0x419a90 系）的旋转矩形域（+0x118/+0x11c 与 flags
    // 0x8000）仍待 StageSim 侧字段落地；kPlayerHitRadius/kEnemyBodyRadius
    // 仅作本作敌机本体近似保留，不再宣称校准。
    static constexpr float kPlayerHitSize[4] = {2.0f, 3.0f, 3.0f, 3.0f};
    static constexpr float kPlayerHitRadius = 3.0f;
    static constexpr float kBulletHitRadius = 4.0f;
    static constexpr float kEnemyBodyRadius = 20.0f;
    static constexpr i32 kInvulnFrames = 240;
    static constexpr i32 kBombFrames = 180;
    static constexpr float kBombDamagePerFrame = 3.0f;
    static constexpr i32 kKillScore = 5000;
    static constexpr i32 kKillPower = 8;
    // 灵界（trance）：
    static constexpr i32 kTranceActivateFrame = 30;   // 0x4057f0：任务计时 ≥0x1e
    static constexpr float kTranceDrainPerFrame = 3.0f;  // 0x4aeca8=3.0【drain 点待 asm 复核】

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
    // 观察接口（r4/harness TH13_RUN_SCENE=1 路径）：装载后补挂事件落盘。
    // 与 battle.load(event_path) 的观察面等价（events_ 置位 → 逐帧
    // sink.set_frame + player 事件），只写 sink，不改变任何战斗机制/相位。
    bool open_event_sink(const char* path);
    const std::string& error() const { return error_; }

    // .sht 弹型域：5 能量档非聚焦（块 0-4）+ 5 能量档聚焦（块 5-9）+
    // 特殊块 10（类别 2，未接入）。
    static constexpr u32 kShtBlocks = 11;
    std::array<std::vector<ShtShotRecord>, kShtBlocks> sht_shots;
    bool sht_shots_loaded = false;

private:
    void move_player(u32 held);
    void update_options(bool reset = false);
    void update_firing(bool held);
    void step_bomb();
    float bomb_damage(StageEnemy& enemy);
    void explode_orb(PlayerBombOrb& orb);
    void fire_shots(i32 frame);
    void step_shots();
    void collide_shots();
    void collide_player();
    void reward_kill(StageEnemy& enemy);
    void apply_damage_pool();   // 0x419a90 命中池：上一帧伤害本帧入命
    void update_item_side();    // 0x42e380/0x438730 玩家只读面装配 + 步进
    bool update_trance(const GameInput& input);  // 0x4057f0/0x405a20/0x405b80
    bool kill_player();
    bool commit_death();
    bool commit_death_normal();
    bool update_player(const GameInput&);
    StageBattleServices* host_ = nullptr;
    SessionState* progress_ = nullptr;
    HudPlayerState* player_ = nullptr;
    HudScoreState* score_ = nullptr;
    i32 fire_frame_ = -1;  // 0x4460d0：停射<0，按住 0..13 循环（0x4732c0 -14）
    std::vector<EnemyScanState> enemy_scan_state_;  // 本帧步进前的敌机命中面
    std::array<std::array<std::array<i32, 2>, 10>, 2> option_offsets_{};
    std::array<std::array<i32, 2>, 8> option_fixed_{};
    std::array<std::array<i32, 2>, 33> movement_history_{};
    i32 option_power_ = -1;
    float bomb_angle_ = -1.5707964f;
    float bomb_center_x_ = 0, bomb_center_y_ = 0;
    float bomb_rotation_speed_ = 0;
    i32 bomb_duration_ = 0;
    i32 character_ = 0;
    bool life_probe_ = false;  // TH13_LIFE_PROBE=1：敌机生命轨迹对拍探针
    bool events_ = false;  // 事件落盘（回放对拍）
    bool sink_open_ = false;  // open_event_sink 已启（load 不清 events_ 的幂等位）
    float player_hit_size_ = 2.0f;  // 0x660+4：0x441dd0 按 kPlayerHitSize 写入
    // 灵界（trance）状态（0x4057f0 任务对象 + 0x405a20/0x405b80 写点）：
    bool trance_protection_ = false;  // 0x405a20 param_1=1：死亡保护灵界
    i32 trance_task_frame_ = 0;       // 任务计时（+0x20，rate 1.0 每帧 +1）
    std::string error_;
};

}  // namespace th13
