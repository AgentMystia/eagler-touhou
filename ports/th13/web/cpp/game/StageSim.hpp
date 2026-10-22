// stage1 敌机/弹幕模拟（wasi harness 无头对照，oracle 合同见 docs/plans/event-contract.md）。
// 敌机模型按原版双子位置结构复刻（0x419450 积分序）：
//   vel 子位置（vm+0x88）与 pos 子位置（vm+0xCC）各自持有极坐标 mover
//   {speed[6], angle[7], radius[8], growth[9], aux[0xD..0xF], mode[0x10]}，
//   主位置（vm+0x44）= vel.pos + pos.pos，每帧经 0x419950 以速度差累加并量化 0.01。
// 插值器两类：极坐标 {angle,speed}（moveVelTime 族，vm+0x3C0/0x410 挂接）与
// 位置三元组（movePosTime 族，vm+0x2F0/0x358），elapsed 每帧 +1（rate 恒 1），
// 完成即写终值并失活（duration 清 0，0x419450 以 duration!=0 作活动门）。
#pragma once

#include "EclResource.hpp"
#include "EclThreads.hpp"
#include "Rng.hpp"
#include "StageEvent.hpp"
#include "Types.hpp"

#include <memory>
#include <string>
#include <vector>

namespace th13 {

// 极坐标插值通道（0x4234b0/0x423900 步进）。
struct StagePolarItp {
    float from_a = 0, from_b = 0, to_a = 0, to_b = 0;
    float duration = 0;
    i32 elapsed = 0;
    i32 mode = 0;
    bool active() const { return i32(duration) > 0; }
    void setup(float dur, i32 new_mode, float fa, float fb, float ta, float tb) {
        duration = dur;
        elapsed = 0;
        mode = new_mode;
        from_a = fa;
        from_b = fb;
        to_a = ta;
        to_b = tb;
    }
};

// 位置插值通道（0x422f80 步进）。字段对应结构 +0x0C 起：
// start（+0x0C，setup 时的子位置）、target（+0x18）、p1/p2（+0x24/+0x30，模式 8 贝塞尔用）。
struct StagePosItp {
    float start[3] = {0, 0, 0}, target[3] = {0, 0, 0};
    float p1[3] = {0, 0, 0}, p2[3] = {0, 0, 0};
    float duration = 0;
    i32 elapsed = 0;
    i32 mode = 0;
    bool active() const { return i32(duration) > 0; }
};

// 单个子位置的极坐标 mover（0x4736a0 每帧产 delta，0x473780 累加并量化）。
struct StageMover {
    float pos[3] = {0, 0, 0};      // [0..2] 累积子位置
    float speed = 0, angle = 0;    // [6]/[7]（圆周模式下 [7]=当前角、[6]=角速度）
    float radius = 0, growth = 0;  // [8]/[9]
    float aux[3] = {0, 0, 0};      // [0xD..0xF]：mode0=每帧位移，mode2=圆心
    u32 mode = 0;                  // [0x10] 低 4 位
    StagePolarItp polar;           // {angle, speed} 插值
    StagePosItp postime;           // 位置插值（活动期间覆写 pos）
};

// ECL 30x ANM 创建请求（0x41b260 分发 → 0x4243e0/0x41fe80：拷脚本模板 →
// 立即 tick → 主池头入链 0x46f4a0）。file = ECL 文件索引（ctl+0x80 表：
// 0=bullet.anm、1=effect.anm、2..=st01 ANIM 名单），script = 文件内脚本号；
// 307/308 特效（308 位置清零 0x4243e0:22-24、307 传敌机 xyz）；
// 303/306 槽位 VM（0x41fe80/case3：重挂先回收旧槽、句柄入敌机+0x120 槽数组、
// slot0 附加尺寸/身份字段）。表现层（StagePresentation::sync）按 ECL 执行
// 序排空。
struct AnmEffectRequest {
    i32 file = 0, script = 0;
    float x = 0, y = 0, z = 0;
    u8 kind = 0;        // 0=特效（307/308）；1=槽位（303/306）
    u16 opcode = 0;     // 槽位路径区分 303/306 的 slot0 附加字段
    i32 slot = 0;
    u32 serial = 0;   // 槽位路径的目标敌机序号
};

// et 发射器槽（0xE0 float 状态的行为子集，事件相关的字段）。
struct StageEt {
    u32 sprite = 0, subtype = 0;       // et[0]/et[1]（602 etSprite）
    float spawn[3] = {0, 0, 0};        // et[2..4]（etOn 时写）
    float angle1 = 0, angle2 = 0;      // et[5]/et[6]（604 etAngle）
    float speed1 = 2.0f, speed2 = 0;   // et[7]/et[8]（605 etSpeed）
    float spin = 0;                    // et[9]（出生自旋半径）
    i16 count = 0, ways = 1;           // +0x364/+0x366（606 etCount）
    i16 aim = 0;                       // +0x368（607 etAim 模式）
    float offset[3] = {0, 0, 0};       // vm+0xF6C+slot*3（603 etOffset）
    float absolute[3] = {0, 0, 0};     // vm+0xF9C+slot*3（absolute[2] > 0.9 走绝对出生）
    void reset();                      // 600 etNew 默认（0x41d573）
};

struct StageEnemy {
    u32 serial = 0;
    i32 script_id = 0;  // +0x11E4，创建时 = 序号（0x418f40:96）
    i32 trace_id = 0;   // +0x11E8 = 创建者 script_id（0x41fcc0:72）
    std::string routine;
    float life = 0;
    StageMover vel, pos;
    float position[3] = {0, 0, 0};  // vm+0x44 主位置
    float age = 0;                  // vm+0x14AC（TIME）
    u32 flags = 0;                  // vm+0x4030：bit18 入场过、bit20 镜像、bit22 活动界限
    float hurtbox[2] = {24.0f, 24.0f};  // +0x110/+0x114（ctor 默认 24）
    float limit_center[2] = {0, 0};     // 504 moveLimit（+0x3F38/+0x3F3C）
    float limit_half[2] = {0, 0};       // +0x3F40/+0x3F44
    StageEt et[8];
    i32 i_slots[4] = {0, 0, 0, 0};      // +0x1474..（I0-I3）
    float f_slots[4] = {0, 0, 0, 0};    // +0x1484..（F0-F3）
    std::unique_ptr<EclThreads> threads;
    bool dead = false;
    // 0x418e00:19-20 的 0x80000 自门（0x41a8a0:30-31）：构造趟置位后，次帧
    // MGR_UPDATE 到达本敌机时空转并清位——root（构造在 MGR_UPDATE 外）因此
    // 比后续敌机多空转一个完整帧（oracle 探针实证 time 序 1,1,2,3）。
    bool tick_gate = false;
    std::string interrupt_label;  // 514 setInterrupt（id 0）注册的标签
    // ---- 交互模式的 ANM 接线记录（无头 oracle 契约不读这些字段）----
    // 302 anmSelect：敌机当前 ANM 文件槽（st01：2=enemy.anm、3=st01enm.anm）。
    i32 anm_file = 0;
    // 303 anmScript / 306 anmRun：槽位脚本（vm+0x120 起槽数组；槽 0 = 本体，
    // 记录 -1 表示清槽）。脚本号是 stage ANM 合并表的全局序号。
    i32 anm_script[4] = {-1, -1, -1, -1};
    // 303/306 真实现的槽位句柄（原版敌机+0x120 起槽数组，0x41fe80/case3
    // 创建后存句柄、重挂时先 0x470110 回收旧 VM）。
    u32 anm_slot_handle[4] = {0, 0, 0, 0};
    // slot0 附加尺寸字段（0x41fe80 → +0x3f30/0x3f34 与 case3 → +0xfcc/0xfcd：
    // VM 精灵像素宽高 × scale，公式 (h*scaleY, w*scaleX)）。事件窗 noop 不写。
    float anm_size_a[2] = {0, 0};
    float anm_size_b[2] = {0, 0};
    // 镜像翻转重挂状态（0x419450:101-152：flags&0x200000 时按 x 朝向符号
    // (+0x270 vs sign(+0x78)) 变化回收槽 0 VM 并以脚本号 +0x278+7 重建——
    // Girl 族 f63 首次移动触发行走动画 VM 的机制）。
    i32 mirror_dir = 0;          // +0x270 朝向符号（构造 0）
    float mirror_prev_x = 0;     // 306 置位时的差分基线（防首拍误判）
    i32 mirror_script_base = 0;  // +0x278（构造 0；脚本号 = base + 7）
};

// 交互模式的弹体实体（fire_et 事件同源的直进弹；无头路径不产实体）。
struct StageBullet {
    float x = 0, y = 0, z = 0, angle = 0, speed = 0;
    u32 sprite = 0, subtype = 0;  // etSprite 的 (类型, 颜色)
    bool alive = true;
};

class StageSim;

// 全局变量供给（0x420380/0x420d00 行为子集；current 指向正在步进的敌机）。
class StageGvars final : public EclVariables {
public:
    StageSim* sim = nullptr;
    bool integer(i32 id, i32& value) override;
    bool floating(i32 id, float& value) override;
    i32* integer_destination(i32 id) override;
    float* float_destination(i32 id) override;

private:
    std::vector<std::pair<i32, i32>> writable_ints_;  // -9949/-9948 等可写全局
};

// 游戏指令（0x41b260 分发器行为子集）。
class StageCommands final : public EclCommands {
public:
    StageSim* sim = nullptr;
    int execute(EclContext& context, u16 opcode) override;

private:
    int create_enemy(EclContext& context, u16 opcode);
    int fire_et(EclContext& context, StageEnemy& enemy);
};

class StageSim {
public:
    // resource_dir：含 st01.ecl/default.ecl/st01bs.ecl 与 ANM 名单核验的 raw 目录。
    bool load(const char* resource_dir, const char* event_path, const char* stage = nullptr);
    // 交互模式装载：资源经 provider 供给（浏览器侧读 /assets），难度位 =
    // 1 << difficulty（0x08 契约 = Lunatic）。event_path 非空时开启事件落盘
    // （回放对拍模式，event-contract.md §7）；空则保持静默（浏览器侧）。
    bool begin(EclResourceProvider& provider, const char* stage_file, i32 difficulty,
               const char* event_path = nullptr);
    // 跑 3600 帧（第 0 帧 = root 创建趟），返回 false 时 error 有值。
    bool run(u32 frames);
    // 交互模式逐帧步进：start() 只做第 0 帧装载趟，此后每帧一次 step_frame()。
    bool start();
    bool step_frame();
    const std::string& error() const { return error_; }
    u32 spawn_count() const { return spawned_; }
    u64 event_count() const { return sink.emitted(); }

    // 供 commands/gvars 使用的会话状态
    EclProgram program;
    GameRandom random{0};       // 种子契约见 event-contract.md §2【待校准】
    StageGvars gvars;
    StageCommands commands;
    StageEventSink sink;
    std::vector<std::unique_ptr<StageEnemy>> enemies;
    std::vector<StageBullet> bullets;  // 交互模式弹体（直进；无头路径恒空）
    StageEnemy* current = nullptr;  // 正在步进的敌机（gvars/commands 用）
    u32 frame = 0;
    float player_x = 192.0f, player_y = 420.0f;  // 契约常量（交互模式每帧覆写）
    i32 boss_slot[3] = {-1, -1, -1};             // mgr+0x5C 起 3 槽（script_id 寻址）
    std::vector<std::string> anim_list;          // st01 的 ANIM 依赖清单（装载核验）
    // ECL 307/308 特效 VM 创建请求（AnmEffectRequest；sync 每帧排空后清）。
    std::vector<AnmEffectRequest> anm_effects;
    // 事件契约模式（stage1/replay 落盘）置 true：307/308 与 stage1 oracle 的
    // NOOP_OPS 同源不解析参数（参数解析对带掩码实参有 RNG 副作用，会移动
    // 事件时间线——replay 事件窗 frame124 分歧实证）；渲染/交互模式保持真实语义。
    bool anm_effects_noop = false;
    // 303/306 槽位 VM 真实现开关（draws 对拍模式置 true：ECL 创建敌机本体
    // VM 入池、表现层敌机段关闭——与 oracle 极小环境同构）。web 暂走表现层
    // 旧路径（双绘制风险），后续轮再切换。
    bool ecl_slot_anm = false;
    std::string error_;
    bool interactive = false;  // 交互模式：弹体实体 + 可写玩家位置（不落事件）
    i32 difficulty_rank = 0x08;
    i32 diff = 3;             // DIFF（DAT_004be7c4 语义：0=E 1=N 2=H 3=L）；
                              // 535/536/548/624/625 的选参下标（begin 时取掩码尾零）

    StageEnemy* find_by_script_id(i32 id);  // 0x417f40（id 0 → null）
    StageEnemy* boss() const;
    // 回放对拍：Boss（MainBoss 创建的 "Boss" 例程敌机）第一非符（首条 513）
    // 的帧号；未到达为 -1。event-contract.md §7.3。
    i32 boss_nonspell_frame = -1;
    u32 boss_main_serial = 0xFFFFFFFF;
    // 519 dialogWait（0x41e407）：msgmgr+0x198==0 即通过——oracle 合成态
    // msgmgr 恒 0 → 原生即过，此处同判（§7.1 对话缺席【待校准】）。
    int boss_dialog_wait() const { return 0; }
    // §7 回放会话才落 boss_op/kill 事件（被动 stage1 窗口流保持 v1 字节不变）。
    bool replay_events = false;
    // 建敌（enmCreate 族与 root 装载共用；含构造内首趟 0x41a8a0）。
    // mirror：0x4183f0:28 的 param2[6]（镜像位，构造趟前置入 flags bit20——
    // 构造趟内 404/405 族按镜像翻转角度）。
    StageEnemy* spawn_enemy(const char* routine, float x, float y, float life, StageEnemy* parent,
                            bool inherit_anm, bool mirror = false);
    // 交互模式伤害：扣血至 0 以下标 dead（下帧遍历摘链，与出界自杀同路）。
    // 回放对拍模式（§7.2）额外：落 kill 事件、清 boss_slot 指向
    // （oracle 侧同式写 mgr+0x5c 槽清零）。
    void damage(StageEnemy& enemy, float amount);
    void clear_bullets() {
        for (auto& bullet : bullets) bullet.alive = false;
    }

private:
    // 0x41a8a0：0x80000 门内干活（积分 → ECL），与 run() 的门/清位配合复刻
    // 0x418e00:19-20 的逐敌机节拍。
    void tick_enemy(StageEnemy& enemy);
    void integrate(StageEnemy& enemy);   // 0x419450 + 0x419950 子集
    bool in_bounds(StageEnemy& enemy);   // 0x419450 出界自杀
    void advance_bullets();              // 交互模式直进弹步进 + 出界回收
    u32 spawned_ = 0;
    u32 next_serial_ = 0;
};

}  // namespace th13
