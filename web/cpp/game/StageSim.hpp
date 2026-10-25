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

#include <functional>
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
// 原版 et 槽基 = 敌机 + slot*0x380；标量区 [0x570,0x950) 为 etNew 的 memset 区
// （0x40e840 以 et+0x570 为参数块基读写）。ex 队列 = param 块 +0x28 起
// （18 × 0x2C 字节），609-612 写入、601 etOn 时整块拷入弹体 +0xbd8。
struct StageEt {
    u32 sprite = 0, subtype = 0;       // et[0]/et[1]（602 etSprite）
    float spawn[3] = {0, 0, 0};        // et[2..4]（etOn 时写）
    float angle1 = 0, angle2 = 0;      // et[5]/et[6]（604 etAngle）
    float speed1 = 2.0f, speed2 = 0;   // et[7]/et[8]（605 etSpeed）
    float spin = 0;                    // et[9]（出生自旋半径）
    // etNew 默认 dword[0x235] = 0x00010001（0x41b260 case 0x61 的
    // 9.18369e-41 denormal 位型）→ count(short)=1、ways(short)=1。
    i16 count = 1, ways = 1;           // +0x364/+0x366（606 etCount）
    i16 aim = 0;                       // +0x368（607 etAim 模式）
    float offset[3] = {0, 0, 0};       // vm+0xF6C+slot*3（603 etOffset）
    float absolute[3] = {0, 0, 0};     // vm+0xF9C+slot*3（absolute[2] > 0.9 走绝对出生）
    // etEx（609-612，0x41b260 case 0x6a → 0x40ffe0 发射时拷入弹体 +0xbd8）的
    // 行为队列。队列每项 0x2c：+0x00..+0x0C 浮点参数 args[0..3]、+0x10..+0x1C
    // 整型参数（args[4..7] 位型存储）、+0x20 op、+0x24 flags。op 是位型行为号，
    // 消费语义见 0x40f030（弹体侧 process_bullet_queue）：flags==0 的项在
    // bb4 含任何活跃行为位时阻塞（串行），flags!=0 的项在 op 位未活跃时继续。
    struct Ex {
        u32 op = 0;                    // 队列 op id（位型：1/2/4/8/0x10/0x40/…）
        u32 flags = 0;
        float args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    } ex[18];                           // 0xc6 dwords / 0x2c bytes per entry
    u32 ex_next = 0;  // enemy+0xf5c + emitter*4: 611/612 append cursor（609/610 同步写）
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
    // Manager state belongs to the enemy, not to any one async ECL thread.
    std::array<EclContext::Interrupt, 8> interrupts{};
    i32 invulnerable = 0;  // +0x3fd0; 515 setInvuln / 513 reset
    bool spell_active = false;  // +0x3f64 bit0
    i32 spell_base_life = 0, spell_life = 0;  // +0x3f50 / +0x3f58 (7x)
    i32 spell_id = -1;
    std::string spell_name;  // decrypted original cp932 bytes
    i32 phase_life_remaining = 0, phase_life_threshold = 0;  // +0x5140/+0x5148
    bool phase_timed_out = false;  // +0x521c bit25
    // ---- ECL 掉落表（506-510，敌机 +0x3F68 区；0x417ad0/0x417b10 消费）----
    // main = dword 0xFDA（510 dropMain / 507 id 0：死亡时产 1 个该 kind 的
    // 道具）；extra[k] = dword 0xFDB+k（507 dropExtra id k+1）：散落 kind
    // k+1 的数量（k = 0..13）；area = 0xFEA/0xFEB（508 dropArea 散布幅度，
    // 构造/506 后为 0）。561 die / 509 dropItems 按表产道具并清表。
    i32 drop_main = 0;
    i32 drop_extra[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    float drop_area[2] = {0, 0};
    // ---- 命中/积分灵域（0x419a90；I 模块伤害池路径消费）----
    // pending_damage = mgr+0x28 命中池（伤害帧写入、下一帧敌机更新入命，
    // G 状态流 f96/f98 池实证）；spirit_accum/spirit_threshold = +0x3f60/
    // +0x3ffc（积分灵累计对阈值）；hit_anim_timer = +0x4004 命中演出计时
    // （伤害帧置 30，>19 门控积分灵产出；递减点未单独取证，按帧递减）。
    float pending_damage = 0;
    float spirit_accum = 0;
    i32 spirit_threshold = 0;
    i32 hit_anim_timer = 0;
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
    // 镜像翻转重挂状态（0x4196d3-0x419827 对齐反汇编实证）：
    // 门 = flags&0x200000 且 x 速度符号（+0x78，±0.03 死区）≠上次朝向
    // （+0x270）；行走 VM 脚本号 = 槽 0 脚本号 + 增量（查表：旧 0 时新 -1→1、
    // 新 0/+1→2；旧 +1→恒 4；旧 -1→恒 3），VM layer 恒 8（+0x278+7），
    // 位置继承旧槽 VM 位置，anmArg=0，尾入池（0x46f3f0）句柄存回槽 0。
    i32 mirror_dir = 0;          // +0x270 上次朝向（构造 0）
    float mirror_prev_x = 0;     // 差分近似 sign(+0x78) 的基线
    // 敌机内嵌 VM+0x278（=敌机结构+0x1464）——敌机族 ANM 创建层基。
    // 构造默认 1（0x418f40 体内 0x419089 mov $1,%edx → 0x419195 写
    // 敌机+0x1464，Unicorn 写 watch 实证每敌机均写 1）；ECL 552
    // （0x41b359，case 0x56 zIndex）为覆盖指令（窗口 st01 未用，st05bs
    // 父子敌机有用例）。两消费点均在内嵌 VM 基址上：303/306 槽位 VM
    // （0x41fe80:0x41ff24）与镜像重挂 VM（0x419450:0x419764——0x418e31
    // add $0x11ec,%eax 后进入）以 [+0x278]+7 建层 ⟹ 默认层 8
    // （trace 实测窗口敌机该字段 = 1）。
    i32 mirror_script_base = 1;
};

// 交互模式的弹体实体（fire_et 事件同源；无头路径不产实体）。
// 基础运动 = 原版 0x40db10 尾部：pos += 速度向量（0x412510 的
// polar(speed, angle) 出生向量）。
// etEx 行为队列（0x40ffe0 发射时 0xc6 int 拷贝入弹体 +0xbd8）按 0x40f030
// 的串行状态机消费（出生时一次 + 每帧一次），已接 st01 全部用到的 op：
//   0x1 减速突进（0x410230）、0x2 透明占位、0x4 steer/accel（0x4102b0）、
//   0x8 螺旋加速（0x410520）、0x10 减停-转向循环（0x4105d0）、
//   0x40 边界反弹（0x410a80 + 0x4107f0/0x4108b0/0x410960/0x4109f0）、
//   0x200 变弹型（0x40f030 case 0x200）、0x400 立即激活（0x411380）、
//   0x2000 子弹发射（0x40f79a，下一条作为角度/条数描述符被双跳消费）、
//   0x4000 透明占位、0x100000 命中位翻转、0x80000000 帧等待（0x40f030
//   case 0x80000000 + 0x40db10 内联倒数）。
// 每帧行为派发顺序 = 0x40ddb5..0x40dea7：op1 → op4 → 0x200000（未用）→
// op8 → 0x10 → 0x40 → 0x20000（未用）→ 0x80000（未用）→ 0x100（未用）→
// wait 位内联块；全部在 pos += vel 之前。
struct StageBullet {
    float x = 0, y = 0, z = 0, angle = 0, speed = 0;
    float vel[3] = {0, 0, 0};   // +0xB7C/+0xB80/+0xB84：每帧位移向量
    u32 sprite = 0, subtype = 0;  // etSprite 的 (类型, 颜色)（0x200 可改写）
    // +0xB90/+0xB94：弹型表 +0x104（0x40e840 出生写；op 0x200 变更弹型时
    // 0x40f030:502-503 重写）。两轴同值。0x444140 以 *0x1 参与命中/擦弹
    // 半径平方；0x443fd0 以 *0.5 参与玩家矩形 AABB。
    float hit_size[2] = {4.0f, 4.0f};
    // +0x20：碰撞/判定控制位（0x40e840:183-188 出生写 = 0x2|0x10；
    // 0x40db10 按 bit1 碰撞门、bit4 圆形/矩形选择、bit6 尺寸缩放分支）。
    u32 flags = 0x12;
    float hit_scale = 1.0f;  // +0x1324（出生 1.0；bit6 分支乘弹体尺寸）
    bool alive = true;
    bool grazed = false;  // +0x20 bit2：0x40db10:229-231 的每弹一次擦弹位
    // ---- etEx 行为状态机（0x40f030 / 0x40db10）----
    StageEt::Ex ex[18];          // +0xBD8 起 0xc6 int 出生拷贝
    i32 ex_cursor = 0;           // +0xBD0：当前消费项（出生 = 种子项，st01 恒 0）
    u32 behavior_flags = 0;      // +0xBB4：已激活行为位（池清零语义）
    u16 state = 1;               // +0xBBE：1 活跃 / 2 延迟首帧（种子 op==2）/ 4 已消
    u32 source_enemy = 0;        // 事件口径：发射者敌机 serial（子弹发射同源）
    i32 delay = 0;               // +0x4C8：出场延迟计数（op 0x400 写 1，仅表现）
    // op 4 通道（0x4102b0 语义）：steer 向量（0x40f030 case4 以
    // 0x412510 = polar(resolved_target, step) 预计算）、时长与计时。
    float steer[3] = {0, 0, 0};  // f60/f64/f68（case4 写 steer xy，z 恒 0）
    i32 ex4_duration = 0;        // f6c
    i32 ex4_timer = 0;           // f3c（FUN_004016c0(0)：int=0）
    // op 1 通道（0x410230）：ef4 int 计时，ef8 float（速率 1，与 int 同步）。
    i32 ex1_timer = 0;
    // op 8 通道（0x410520 语义，f94/f98/fb4/f84）。
    float ex8_speed_delta = 0;  // f94
    float ex8_angle_delta = 0;  // f98
    i32 ex8_duration = 0;       // fb4
    i32 ex8_timer = 0;          // f84
    // op 0x10 通道（0x4105d0；spawn 写 fdc/fe0/ffc/1000/1004/1008/100c）。
    float ex10_speed = 0;       // fdc：转向后的恢复速度
    float ex10_angle = 0;       // fe0：目标角（mode 依 f6）
    i32 ex10_duration = 0;      // ffc
    i32 ex10_repeats = 0;       // 1000（i5）
    i32 ex10_count = 0;         // 1004：已完成转向次数
    i32 ex10_mode = 0;          // 1008（f6）
    i32 ex10_timer = 0;         // fcc
    float ex10_time = 0;        // fd0（float 计时，速率 1）
    // op 0x40 通道（0x410a80；spawn 写 1024/102c/1030/1044/1048/1050）。
    float ex40_speed = 0;       // 1024（<-990 不覆盖）
    float ex40_w = 448.0f;      // 102c（i5 bit5 未置时默认 448）
    float ex40_h = 448.0f;      // 1030
    i32 ex40_bounces = 0;       // 1044
    i32 ex40_limit = 0;         // 1048（i4）
    u32 ex40_mode = 0;          // 1050（i5：bit1 顶、bit2 底、bit8 右、bit4 左、bit10 只计数）
    // op 0x80000000 等待通道（0x40f030 case + 0x40de74 内联倒数）。
    i32 wait_timer = 0;         // 1058（int = i4-1，倒计镜像）
    float wait_time = 0;        // 105c（float = i4，>0 期间阻塞队列恢复）
    // op 0x800000 出生快照（0x1264..0x1274；op 0x10 mode 2/3 消费）。
    float saved_speed = 0, saved_angle = 0, saved_pos[3] = {0, 0, 0};
};

class StageSim;

// 道具宿主接口（掉落/消弹转换的同步调用面；由 ItemManager 实现，StageBattle
// 注入）。对应原版 0x42f020（普通/取消道具）与 0x438ec0（带分值道具，
// 内部按 item 序号 %4 选象限并各抽 1 次 signed_unit + 1 次 unit）。
// 即时同步调用以保持原版 RNG 次序（0x417b10/0x4114f0 的抽次不可延迟）。
struct StageItemHost {
    virtual ~StageItemHost() = default;
    // 0x42f020(pos, kind, angle, speed)：kind 1..9 常规/取消道具（无 RNG）、
    // kind 9 = 取消道具。angle/speed 由调用方给（掉落/消弹 = -π/2, 2.2）。
    virtual void spawn_item(float x, float y, float z, i32 kind, float angle, float speed) = 0;
    // 0x438ec0(mgr, kind0, kind, pos, base_speed, 1e-45)：kind = 0..4
    // （ECL 掉落 10..14 减 10），每次调用抽 signed_unit（象限角）+ unit
    // （速度 = unit*3.5 + base_speed）；pos 先按 x∈[-192,192]、y∈[0,384] 钳制。
    virtual void spawn_value_item(i32 kind, float x, float y, float z, float base_speed) = 0;
    // 0x4302a0：616 etClear 的转换点登记（道具管理侧决定语义）。
    virtual void on_et_clear(float x, float y, float z) = 0;
};

// 对话系统宿主接口（ins_518/519 的接口位）：原版 0x428820(param>=0) 建
// 0x1a8 对话对象挂 stage ctx+0x198，519 dialogWait 以 +0x198==0 放行。
// 无头契约路径（stage1/replay 窗）不注入——msgmgr 合成态恒 0 的旧行为
// 保持逐字节不变（event-contract.md §7.1【待校准】）；交互模式由 RunScene
// 注入 MsgPlayer 实现。
struct StageDialogHost {
    virtual ~StageDialogHost() = default;
    virtual void open_dialog(i32 entry) = 0;     // 0x428820 else 分支
    virtual bool dialog_active() const = 0;      // stagectx+0x198 != 0
};

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
    GameRandom owned_random_{0};

public:
    explicit StageSim(GameRandom* shared_random = nullptr)
        : random(shared_random ? *shared_random : owned_random_) {}
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
    // Production shares the original single game RNG with ANM. Standalone
    // harnesses own one RNG; references never mirror/copy a second state.
    GameRandom& random;
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
    i32 rank = 0;             // independent DAT_004be7cc; original .data initial value

    StageEnemy* find_by_script_id(i32 id);  // 0x417f40（id 0 → null）
    StageEnemy* boss() const;
    // 回放对拍：Boss（MainBoss 创建的 "Boss" 例程敌机）第一非符（首条 513）
    // 的帧号；未到达为 -1。event-contract.md §7.3。
    i32 boss_nonspell_frame = -1;
    u32 boss_main_serial = 0xFFFFFFFF;
    // 519 dialogWait（0x41e407）：msgmgr+0x198==0 即通过——oracle 合成态
    // msgmgr 恒 0 → 原生即过，此处同判（§7.1 对话缺席【待校准】）。
    int boss_dialog_wait() const { return 0; }
    // 对话宿主（交互模式注入；空 = 对话缺席：518 只做清场尾块、519 即过）。
    StageDialogHost* dialog_host = nullptr;
    // §7 回放会话才落 boss_op/kill 事件（被动 stage1 窗口流保持 v1 字节不变）。
    bool replay_events = false;
    // 敌机销毁回收钩（step_frame 摘链前逐敌机调用；交互模式由宿主注入，
    // 无头契约路径为空——槽位 VM 句柄仅句柄化路径持有）。
    std::function<void(u32)> retire_sink;
    // 道具宿主（掉落/消弹转换；StageBattle 注入 ItemManager，可空 = 无道具
    // 且不抽道具 RNG——原版空池同义，事件契约路径保持空）。
    StageItemHost* items = nullptr;
    // DAT_004be830 的移植代理（509 dropItems 的 (flags & 0x30) != 0x20 门）。
    // 初值 0 = 常规对局（掉落执行）；0x20 态未被任何调用方建模。
    u32 mode_flags = 0;
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
    // ---- etEx 行为队列（0x40f030 / 0x40db10 / 0x40f79a / 0x4114f0）----
    // 消费弹体 ex 队列到首个阻塞项（op 0 / flags 门 / op 位已活跃）。
    // spawn_time=true 时为 0x40e840 尾的出生消费（事件窗 RNG 次序敏感）。
    void process_bullet_queue(StageBullet& bullet, bool spawn_time);
    // 0x40db10 的弹体每帧更新（状态机 + 行为派发 + 位移 + 出界回收）。
    void advance_bullets();
    // 0x417ad0/0x417b10：按敌机 ECL 掉落表产道具（items 为空时仅抽表内
    // 固定的 RNG 次数，与原版空道具池语义一致）。
    void drop_table(StageEnemy& enemy);
    // 0x4114f0：615/616 半径消弹；spawn_cancel = 615（场内弹产取消道具）。
    void cancel_bullets_radius(const StageEnemy& enemy, float radius, bool spawn_cancel);
    // 0x40ffe0/0x40e840 的发射内核：按 et 视图逐 (way, idx) 产事件与弹体
    // （StageCommands::fire_et 与 etEx op 0x2000 子弹发射共用）。
    // slot = 事件口径的发射器号（601 的实参；0x2000 子发射按 oracle 的
    // et 指针回退口径恒 0）。ex_seed = 弹体 ex 队列起始项（601 恒 0 =
    // et dword 0x23A 默认；0x2000 子弹 = 打包 i4 的 byte0）。
    void emit_et(const StageEt& et, u32 enemy_serial, u32 slot, const float spawn[3], float aim,
                 u32 ex_seed = 0);

private:
    // 0x40db10 的逐帧行为派发（op1/op4/op8/0x10/0x40/wait），行为可能改写
    // vel/angle/speed，位移随后统一做。
    void step_bullet_behaviors(StageBullet& bullet);
    // 0x41a8a0：0x80000 门内干活（积分 → ECL），与 run() 的门/清位配合复刻
    // 0x418e00:19-20 的逐敌机节拍。
    void tick_enemy(StageEnemy& enemy);
    // 0x4218f0: health threshold first, then the first timed slot; never ECL time.
    bool interrupt_enemy(StageEnemy& enemy);
    void integrate(StageEnemy& enemy);   // 0x419450 + 0x419950 子集
    bool in_bounds(StageEnemy& enemy);   // 0x419450 出界自杀
    u32 spawned_ = 0;
    u32 next_serial_ = 0;
};

}  // namespace th13
