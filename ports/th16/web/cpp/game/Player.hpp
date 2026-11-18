// th16 Player——原版 DAT_004a6ef8（0x2c828 字节，0x441c60 创建 / 0x440fb0 初始化 / 0x441740 首帧复位 /
// 更新链 0x16 → 0x443720 → 0x442560 / 绘制链 0x1d → 0x443730）。
//
// 原版布局（本文件字段注释给出偏移，harness `game-run` 按同一口径导出）：
//   +0x004 更新链  +0x008 绘制链  +0x00c pl0N.anm（槽 9）  +0x010 pl0Nsub.anm（槽 0x1e）  +0x014 自机 VM（0x5fc，不入池）
//   +0x610 实体 A（0x16090）：+0x610 pos f32×3  +0x61c/+0x620 定点 pos（×128）  +0x624/+0x638/+0x64c 三个 Timer
//          +0x660 子机 ×4（0xe4）  +0x9f0 子季节子机 ×8  +0x1110 自机弹 ×256（0xc0）  +0xd110 上次伤害源下标
//          +0xd114 伤害源 ×257（0x94）  +0x165a8 状态（0 入场 1 正常 2 复活等待 3 4 被弹）  +0x165ac 低速判定点 VM id
//          +0x165b0 第二特效 VM id  +0x165b4 其 Timer  +0x165c8 低速标志  +0x165cc/+0x165e0 射击 Timer  +0x165f4 子机数
//          +0x16638 无敌 Timer  +0x1664c 标志  +0x16650..+0x1665c 四档速度（int×128）  +0x16660 速度 f32×3
//          +0x1666c 上次非零速度  +0x16678 整数速度  +0x16680 子机跟随滞后（0x1e）  +0x16684 收拢计数
//          +0x16688 速度倍率  +0x1668c 外部推力 ×3  +0x16698 火力等级  +0x1669c 子季节子机数
//   +0x166a0 实体 B（同构，复活/残像用；本阶段未接）
//   +0x2c730 判定矩形  +0x2c748 半径常量（判定/收点/擦弹各 ×0.5，z=5）  +0x2c76c pos 副本  +0x2c778 上次移动
//   +0x2c780 方向（DAT_00492c20 表下标）  +0x2c788/+0x2c78c .sht  +0x2c798 缩放插值  +0x2c7c0/+0x2c7c8 缩放
//   +0x2c7dc 第二组判定矩形（收点 / 擦弹 / 收点整幅）
#pragma once
#include "AnmVm.hpp"
#include "Motion.hpp"
#include "ShotTable.hpp"
#include "Types.hpp"

namespace th16 {

class AnmManager;
class AnmRenderer;
struct AnmFile;
class EnemyManager;
struct GameGlobals;
struct InputState;
struct Rng;

// 自机弹（0xc0 字节；+0x1110 起 ×256）。0x444e10 生成 / 0x4456d0 逐帧推进 / 伤害源随之同步。
struct PlayerBullet {
    u32 flags = 0;          // +0x00 bit0：已登记伤害源（tick 每帧把 pos/角度/尺寸/伤害抄进伤害源）bit1：低速发射
    i32 index = 0;          // +0x04 池下标（伤害源 +0x88 回指）
    u32 vm = 0;             // +0x08 弹 VM id（pl0N.anm 脚本 .sht+0x22（主：+5），layer -1）
    Timer timer;            // +0x0c
    u8 pad20[0x28] = {};    // +0x20
    MotionState motion;     // +0x48（pos +0x48 / speed +0x60 / angle +0x64 / velocity +0x7c / mode +0x88）
    i32 active = 0;         // +0x8c（1 活动；2 = 激光等由回调改）
    i32 target = 0;         // +0x90 追踪目标敌人 id（回调 1）
    u32 pad94[2] = {};      // +0x94
    i32 damage = 0;         // +0x9c（.sht +2）
    float hit_w = 0;        // +0xa0（.sht +0xc）
    float hit_h = 0;        // +0xa4（.sht +0x10）
    u32 pada8 = 0;          // +0xa8
    u32 shot_id = 0;        // +0xac  (set<<8 | entry)，子季节组另加 0x10000
    i32 damage_source = 0;  // +0xb0  伤害源下标（1 起；0 = 无）
    u32 padb4[3] = {};      // +0xb4
};
static_assert(sizeof(PlayerBullet) == 0xc0, "PlayerBullet 为 0xc0 字节");

// 伤害源（0x94 字节；+0xd114 起 ×256，下标 1 起；敌人碰撞时读）。0x444b20 分配；
// 0x442560 每帧：运动推进（自机弹的为 mode 0、速度 0）、+0x04 += +0x08、角度 += 角速度、寿命 Timer 回退，< 1 失活。
struct DamageSource {
    u32 flags = 0;          // +0x00 bit0 活动（分配时清 bit1/bit2）
    float f04 = 0;          // +0x04（每帧 += f08）
    float f08 = 0;          // +0x08
    float angle = 0;        // +0x0c（每帧 += angular_speed 并折回）
    float angular_speed = 0;// +0x10
    float size_x = 0;       // +0x14（自机弹 hit_w）
    float size_y = 0;       // +0x18（hit_h）
    MotionState motion;     // +0x1c（pos +0x1c；分配时整体清零后写 pos）
    Timer timer;            // +0x60（寿命 9999999，每帧 untick）
    i32 damage = 0;         // +0x74
    i32 f78 = 0;            // +0x78
    i32 f7c = 0;            // +0x7c（9999999 → 自机弹改 10000000）
    i32 f80 = 0;            // +0x80（1）
    i32 f84 = 0;            // +0x84（每帧清 0）
    i32 owner = 0;          // +0x88 自机弹池下标
    i32 f8c = 0;            // +0x8c（分配不写）
    i32 kind = 0;           // +0x90（1 普通；回调 4（Aya 主弹）置 2）
};
static_assert(sizeof(DamageSource) == 0x94, "DamageSource 为 0x94 字节");

// 子机（0xe4 字节；+0x660 起 ×4，+0x9f0 起 ×8）
struct PlayerOption {
    i32 active = 0;        // +0x00（2 = 活动）
    i32 target_x = 0;      // +0x54（定点）
    i32 target_y = 0;      // +0x58
    i32 x = 0;             // +0x5c
    i32 y = 0;             // +0x60
    i32 offset_x = 0;      // +0x64 非低速偏移（定点）
    i32 offset_y = 0;      // +0x68
    i32 focus_offset_x = 0;// +0x6c 低速偏移
    i32 focus_offset_y = 0;// +0x70
    float angle = 0;       // +0x4c 子机朝向（.sht 角度 ≥ 995 的弹取此值；th16 数据不使用，恒 0）
    u32 vm = 0;            // +0xb0 子机 VM id（世界链表尾，layer 0xe）
    u32 vm_front = 0;      // +0xb4 满火力附加 VM id（0x426160 世界链表头，layer -1）
    i32 index = 0;         // +0xd0
    i32 snap = 0;          // +0xd4（1 = 下一帧直接吸附到目标）
    i32 callback = 0;      // +0xdc（移动回调表下标；th16 恒 0）
};

// 其它管理器尚未接入时由宿主提供的外部状态（原版直接读全局对象）
struct PlayerWorld {
    i32 enemy_count = 0;        // DAT_004a6dc0->0x18c：场上敌人数（为 0 时禁用低速与射击）
    bool dialogue = false;      // DAT_004a6dcc->0x1c8 != 0：对话中
    bool stage_clear = false;   // DAT_004a6dd4->0x88 & 0x4000
};

class Player {
public:
    enum State : i32 { kEnter = 0, kNormal = 1, kRespawnWait = 2, kState3 = 3, kDying = 4 };
    enum : u32 {
        kFlagOptionsGathered = 0x2,   // +0x1664c bit1：子机收拢到自机（被弹后）
        kFlagSilentHit = 0x8,         // bit3：被弹不播音效 2
        kFlagNoShot = 0x4,            // bit2
        kFlagScaled = 0x10,           // bit4：缩放模式（+0x2c7c8）
        kFlagBlinkMode = 0x20,        // bit5
    };
    static constexpr i32 kOptionLag = 0x1e;
    static constexpr int kOptionCount = 4;
    static constexpr int kSubOptionCount = 8;

    AnmVm vm;                     // +0x14
    AnmFile* anm = nullptr;       // +0x0c
    AnmFile* sub_anm = nullptr;   // +0x10
    ShotTable sht;                // +0x2c788
    ShotTable sub_sht;            // +0x2c78c

    Vec3 pos;                     // +0x610
    i32 fixed_x = 0, fixed_y = 0; // +0x61c / +0x620
    Timer state_timer;            // +0x624
    Timer timer2;                 // +0x638（低速解禁：current ≥ 4）
    Timer timer3;                 // +0x64c（射击解禁：current > 0x13）
    PlayerOption options[kOptionCount];        // +0x660
    PlayerOption sub_options[kSubOptionCount]; // +0x9f0
    i32 state = kEnter;           // +0x165a8
    u32 focus_vm = 0;             // +0x165ac（effect.anm 脚本 0x1a，layer 0xe）
    u32 effect_vm = 0;            // +0x165b0（effect.anm 脚本 0x1b）
    Timer effect_timer;           // +0x165b4
    i32 focus = 0;                // +0x165c8
    Timer shot_timer;             // +0x165cc
    Timer shot_timer2;            // +0x165e0
    i32 option_count = 0;         // +0x165f4
    Timer invincible;             // +0x16638
    u32 flags = 0;                // +0x1664c
    i32 speed = 0, speed_focus = 0, speed_diag = 0, speed_diag_focus = 0;  // +0x16650..+0x1665c
    Vec3 velocity;                // +0x16660
    Vec3 last_velocity;           // +0x1666c
    i32 velocity_ix = 0, velocity_iy = 0;  // +0x16678 / +0x1667c
    i32 option_lag = kOptionLag;  // +0x16680
    i32 gather_counter = 0;       // +0x16684
    float speed_mult = 1.0f;      // +0x16688
    Vec3 push;                    // +0x1668c
    i32 power_level = 0;          // +0x16698
    i32 sub_count = 0;            // +0x1669c
    i32 last_damage_index = 0;    // +0xd110

    Vec3 rect_min, rect_max;      // +0x2c730 / +0x2c73c 判定框
    Vec3 hit_radius;              // +0x2c748（判定 ×0.5, ×0.5, 5）
    Vec3 item_radius;             // +0x2c754（收点 ×0.5）
    Vec3 graze_radius;            // +0x2c760（擦弹 ×0.5）
    Vec3 pos_copy;                // +0x2c76c
    i32 last_move_x = 0, last_move_y = 0;  // +0x2c778 / +0x2c77c
    i32 direction = 0;            // +0x2c780
    i32 shot_misc = 0;            // +0x2c790
    u8 shot_misc_b = 0;           // +0x2c794
    Interp<float> scale_interp;   // +0x2c798
    i32 scale_active = 0;         // +0x2c7c0
    float scale = 1.0f;           // +0x2c7c8
    float scale2 = 1.0f;          // +0x2c7cc
    Vec3 item_rect_min, item_rect_max;    // +0x2c7dc / +0x2c7e8
    Vec3 graze_rect_min, graze_rect_max;  // +0x2c7f4 / +0x2c800
    Vec3 item_rect2_min, item_rect2_max;  // +0x2c80c / +0x2c818
    static constexpr int kBulletCount = 256;
    static constexpr int kDamageSourceCount = 256;
    PlayerBullet bullets[kBulletCount];                // +0x1110（0x4456d0 按 4 个一组扫描空位）
    DamageSource damage_sources[kDamageSourceCount];   // +0xd114（下标 i 对应原版 1 起的 i+1）
    i32 option_cooldown[16] = {};                      // +0x165f8：.sht 类型 2 弹按子机 [opt-1 (+8 子季节)] 记录发射时火力等级
    std::vector<i32> sfx_queue;                        // 0x45e1f0 音效登记（宿主每帧取走）
    std::vector<i32> sfx_stop_queue;                   // 不射击时对 0x1e / 0x37 的停止标记（DAT_004d9e5c = -1）
    EnemyManager* enemies = nullptr;                   // DAT_004a6dc0（追踪弹索敌 0x425240 用）

    // 0x441c60 + 0x440fb0：资源已由调用方装入；返回 false 表示 .sht 缺失
    bool init(AnmManager& manager, GameGlobals& globals, AnmFile* player_anm, AnmFile* sub_player_anm,
              const u8* sht_bytes, u32 sht_size, const u8* sub_sht_bytes, u32 sub_sht_size);
    // 0x441740（0x42dc50 首帧）：状态置 1、计时器复位、特效 VM 删除、子机中断 2 并重建
    void reset(AnmManager& manager, GameGlobals& globals);
    // 0x4440e0：按火力等级 / 季节量表重建子机 VM
    void setup_options(AnmManager& manager, GameGlobals& globals);
    // 0x442560：更新链 0x16。geometry：0x468c00 出界判定用的屏幕几何（DAT_004d9d34/50/54）
    void update(AnmManager& manager, GameGlobals& globals, const InputState& game_input, const PlayerWorld& world,
                AnmFile* effect_anm, const AnmRenderer& geometry, float game_speed);
    // 0x443730：绘制链 0x1d 的逻辑部分（pos → VM entity_offset，flags 0x40000）
    void prepare_draw();
    // 0x443f10：被弹——音效 2、effect.anm 脚本 0x1d、状态 4（决死猶予）、无敌 6 帧、主 VM 重播脚本 0
    void hit(AnmManager& manager, AnmFile* effect_anm);
    // 0x444cf0：擦弹——计数封顶 99999999、effect.anm 脚本 0x18 于中点、音效 0x2a（得点道具由调用方生成）
    void graze(AnmManager& manager, AnmFile* effect_anm, GameGlobals& globals, const Vec3& bullet_pos);
    // 0x445a30：对位置 enemy_pos、圆形半径 radius 的敌人（enemy_id）结算全部活动伤害源，返回本帧总伤害
    i32 apply_damage_sources(AnmManager& manager, const Vec3& enemy_pos, float radius, u32 enemy_id);

    std::string error;

private:
    float game_speed_ = 1.0f;                        // DAT_004a5788（update 入口抄入，供 0x444e10 用）
    void set_position(float x, float y);            // 0x440e40
    void recompute_rects_init();                     // 0x440fb0 尾部的判定框
    void move(AnmManager& manager, const InputState& game_input, const PlayerWorld& world, AnmFile* effect_anm,
              float game_speed);                     // 0x441cf0
    void update_options(AnmManager& manager, PlayerOption* list, int count);  // 0x442380
    void play_sprite(AnmManager& manager, int script);   // 0x407b20 + 0x45f980
    void kill_vm(AnmManager& manager, u32& id);          // 0x46f1c0 并清 id
    void interrupt_options(AnmManager& manager);         // 0x440dc0
    void reset_shot_timers();
    void shoot(AnmManager& manager, GameGlobals& globals, const InputState& game_input, float game_speed);  // 0x4455d0
    void start_shot_timer(i32 t);                                         // 0x440d50
    void fire_sets(AnmManager& manager, GameGlobals& globals, i32 frame, i32 frame2);  // 0x445470
    bool spawn_bullet(AnmManager& manager, GameGlobals& globals, u32 shot_id);          // 0x445360 → 0x444e10
    i32 alloc_damage_source(const Vec3& pos, float angle, i32 lifetime, i32 damage, float size_x, float size_y);  // 0x444b20
    void tick_bullets(AnmManager& manager, const AnmRenderer& geometry, float game_speed);  // 0x4456d0
    void release_bullet(PlayerBullet& b);                                 // 池项失活 + 伤害源 bit0 清零
    i32 hit_bullet_default(AnmManager& manager, PlayerBullet& b);         // 0x445e20
    const ShotEntry* shot_entry(u32 shot_id) const;                       // (set<<8|entry)(+0x10000 子季节) → .sht 项
    u32 spawn_option_vm(AnmManager& manager, AnmFile& file, int script);  // 0x4440e0 内联生成（世界链表尾，layer 0xe）
};

}  // namespace th16
