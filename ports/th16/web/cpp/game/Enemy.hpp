// th16 EnemyManager（DAT_004a6dc0，0x190）+ Enemy（0x574c）：
//   0x41b340 创建 / 0x41ae70 装 ECL、登记更新链 0x1a（0x41b4f0→0x41b3d0）
//   0x41aa70 生成（含立即 0x41d1e0） / 0x41d1e0→0x41d2e0 每帧（运动 0x41bb50 → ECL 0x473bc0 → 碰撞 0x41c330）
//   敌方 ECL 操作码 ≥300：0x41dcb0（本阶段覆盖生成/运动/判定/标志等 st01 前段所需）
#pragma once
#include "Bullet.hpp"
#include "Ecl.hpp"
#include "Motion.hpp"
#include "Types.hpp"
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace th16 {

class AnmManager;
class Archive;
struct GameGlobals;
class Player;

struct EnemySpawnArgs {
    Vec3 pos;
    float life = 0;          // +0x14；≥1000 → flags |= 0x40000000
    i32 score = 0;
    i32 item = 0;
    u32 mirror = 0;          // → flags bit 19
    u32 extras[8]{};
};

class Enemy;

class Enemy final : public EclVariables, public EclThreadHost, public EclCommands {
public:
    Enemy* self = this;
    EclContext primary;
    std::list<std::unique_ptr<EclContext>> async_threads;
    EclProgram* program = nullptr;
    class EnemyManager* manager = nullptr;
    GameGlobals* globals = nullptr;
    Player* player = nullptr;
    Rng* rng = nullptr;

    u32 id = 0;                 // +0x5740
    u32 flags = 0;              // EnemyInf+0x4060 / Enemy+0x526c
    Vec3 pos;                   // EnemyInf+0x44
    MotionState motion1;        // EnemyInf+0x88
    MotionState motion2;        // EnemyInf+0xcc
    Vec3 hitbox;                // EnemyInf+0x110（op 500）
    Vec3 hurtbox;               // EnemyInf+0x11c（op 501）
    Timer invuln;               // EnemyInf+0x3ffc（生成时 = 2）
    Timer life_timer;           // 年龄计时（生成时 current=0x3c）
    i32 life = 0;               // EnemyInf+0x3f74 / Enemy+0x5180 / ECL -9954
    i32 life_max = 0;           // +0x3f78 / +0x5184
    i32 life_budget = 0;        // +0x3f80 / +0x518c（符卡 7 倍血量池）
    i32 phase_life = 0;         // +0x3f84 / +0x5190
    u32 life_flags = 0;         // +0x3f8c / +0x5198（bit0 = 符卡 7 倍减伤）
    i32 anm_slot = 0;           // +0x264
    bool has_main_vm = false;   // +0x124 非零标记
    Timer season_drop_timer;    // EnemyInf+0x403c / Enemy+0x5248（op 1000 限时季节掉落计时）
    i32 season_drop_duration = 0x3c; // +0x4050 / +0x525c（op 1000 arg0）
    i32 season_drop_max = 0;    // +0x3fd0 / +0x51dc（op 1000 arg1）
    i32 season_drop_min = 1;    // +0x4054 / +0x5260（op 1000 arg2）
    i32 hit_flash_timer = 0;    // +0x3ff0（受击闪烁/音效节流 4 帧）
    i32 hit_sfx = -1;           // +0x3ff8
    i32 death_sfx = -1;         // +0x3fe4 / +0x51f0（(id & 1) + 3）
    i32 boss_slot = -1;
    float rate = 0;             // +0x14f0
    Vec2 sprite_size{};         // EnemyInf+0x3f58..0x3f5c（op 306 slot=0 时置 32×32）
    Vec2 bound_center{};        // EnemyInf+0x3f60..0x3f64（op 504）
    Vec2 bound_size{};          // EnemyInf+0x3f68..0x3f6c（op 504）
    i32 int_regs[4]{};          // EnemyInf+0x1498 旁整数槽（部分与 life 重叠，先独立）
    float float_regs[4]{};
    float temp_regs[4]{};       // EnemyInf+0x14b8..0x14c4（-9935..-9932）
    Interp<Vec3> pos_interp{};     // EnemyInf+0x2f8（op 401）
    Interp<float> angle_interp{};  // EnemyInf+0x3c8（op 405/412）
    Interp<float> speed_interp{};  // EnemyInf+0x3f8（op 405/412）

    // 弹幕发射器（ECL 600..641 / 526；0x41b580 整块清零）
    static constexpr int kShooterSlots = 16;
    EtShooter shooters[kShooterSlots];        // EnemyInf+0x598（0x380 × 16）
    Vec3 shooter_offset[kShooterSlots];       // EnemyInf+0x3dd8（ECL 603 / 626 相对偏移）
    Vec3 shooter_pos2[kShooterSlots];         // EnemyInf+0x3e98（ECL 628 绝对坐标；z > 0.9 表示启用）
    i32 shooter_ex_counter[kShooterSlots]{};  // EnemyInf+0x3d98（ECL 611 / 612 递增下标）
    float safe_radius_sq = 0;                 // EnemyInf+0x4074（ECL 526：发射时自机近身安全半径²）
    BulletManager* bullets = nullptr;         // DAT_004a6dac
    std::string error;
    bool alive = true;

    const char* routine_name() const noexcept;

    // EclVariables：0x423810 / 0x423f80（先覆盖 st01 前段用到的）
    bool integer(i32 id, i32& value) override;
    bool floating(i32 id, float& value) override;
    i32* integer_destination(i32 id) override;
    float* float_destination(i32 id) override;

    bool spawn(EclContext& source, i32 id, u32 skipped_arguments) override;
    EclContext* find(i32 id) override;
    void terminate_children() override;
    int execute(EclContext& context, u16 opcode) override;

    void reset_context(EclContext& ctx, i32 routine);
    bool init(EnemyManager& mgr, EclProgram& program, const char* sub, const EnemySpawnArgs& args,
              GameGlobals& globals, Player* player, Rng& rng, u8 difficulty);
    // 0x41d1e0 / 0x41d2e0：返回 false → 应销毁
    bool update(float game_speed);
};

class EnemyManager final : public EclResourceProvider {
public:
    EclProgram program;
    std::list<std::unique_ptr<Enemy>> enemies;
    u32 next_id = 1;
    i32 enemy_count = 0;        // +0x18c
    Timer timer;
    i32 enemy_control = 99999;  // +0x8c
    i32 global_ints[4]{};       // +0x0c..0x18（-9926..-9923）
    float global_floats[8]{};   // +0x1c..0x38（-9922..-9915）
    i32 counters[3]{};          // +0x3c..0x44（-9949..-9947）
    u32 boss_ids[3]{};          // +0x80..0x88
    std::vector<i32> sfx_queue; // Enemy 触发的 0x45e1f0 音效（受击 0x22/0x23、死亡 3/4）
    BulletManager* bullets = nullptr;  // DAT_004a6dac（GameSession 在 BulletManager 创建后绑定）
    AnmManager* animations = nullptr;
    GameGlobals* globals = nullptr;
    Player* player = nullptr;
    Rng* rng = nullptr;
    Archive* archive = nullptr;  // 前向：用函数注入读档
    std::string error;
    using ReadFn = bool (*)(void*, const std::string&, std::vector<u8>&);
    void* read_user = nullptr;
    ReadFn read_fn = nullptr;
    using AnmFn = bool (*)(void*, u32, const std::string&);
    void* anm_user = nullptr;
    AnmFn anm_fn = nullptr;

    bool read(const std::string& name, std::vector<u8>& data) override {
        return read_fn && read_fn(read_user, name, data);
    }
    bool animation(u32 slot, const std::string& name) override {
        return !anm_fn || anm_fn(anm_user, slot, name);
    }

    // 0x41b340 / 0x41ae70
    bool create(const char* ecl_name, GameGlobals& globals, Player* player, Rng& rng,
                AnmManager& animations, u8 difficulty);
    // 0x42dc50 段：生成 "main"
    bool spawn_main();
    // 0x41b3d0
    void update(float game_speed);
    Enemy* spawn_enemy(const char* sub, const EnemySpawnArgs& args);
    Enemy* find_id(u32 id) noexcept;
};

}  // namespace th16
