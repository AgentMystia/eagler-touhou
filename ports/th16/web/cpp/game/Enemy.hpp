// th16 EnemyManager（DAT_004a6dc0，0x190）+ Enemy（0x574c）：
//   0x41b340 创建 / 0x41ae70 装 ECL、登记更新链 0x1a（0x41b4f0→0x41b3d0）
//   0x41aa70 生成（含立即 0x41d1e0） / 0x41d1e0→0x41d2e0 每帧（运动 0x41bb50 → ECL 0x473bc0 → 碰撞 0x41c330）
//   敌方 ECL 操作码 ≥300：0x41dcb0（本阶段覆盖生成/运动/判定/标志等 st01 前段所需）
#pragma once
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
    i32 life = 0;               // EnemyInf+0x1498 / ECL -9985
    i32 life_max = 0;
    i32 anm_slot = 0;           // +0x264
    i32 drop_timer = 0;         // +0x4050（op 1000）
    i32 drop_interval = 0;
    i32 drop_type = 0;
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
