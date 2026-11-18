// th16 EffectManager——原版 DAT_004a6db8（0x2020 字节，0x418a30 创建 / 0x4187d0 装 bullet.anm 槽 7 与 effect.anm 槽 8、
// 更新链 0x1f / 绘制链 0x26）。
//
// 职责：
//   +0x18 游标 / +0x1c id 表 ×0x400（0x40e6c0 找空槽；0x41aa00 登记任意 VM id，敌机 ECL 307/308/315/338/339 的
//   自由 VM 与 ECL 334 的特效都经此登记，供关卡结束统一回收）。
//   效果种类表 DAT_004a2250（0x20 字节一行：i16 anm 槽、i16 脚本、init fn、update/draw/delete/interrupt 回调下标、+0x5d4/+0x5d8）：
//     0 过场（0x4071a0 / 0x407330）  1 充能环（0x405670 / 0x405700）  2、3 爆散粒子（0x406510 / 0x406930 / 0x406690）
//   0x418af0：ins_508(kind) 把回调挂到调用 VM；ECL 334（0x40e730）/ 过场 0x418ba0 则先按表生成新 VM。
//   回调经 AnmVmHooks 由 AnmManager 在 0x45f980（update）/ 0x46eab0（delete）/ 0x46f0b0（interrupt）处回调。
#pragma once
#include "AnmManager.hpp"
#include "Types.hpp"

namespace th16 {

class EffectManager final : public AnmVmHooks {
public:
    static constexpr int kIdSlots = 0x400;
    static constexpr int kKinds = 4;

    // 0x4187d0 装载后的文件（槽 8 / 槽 7）；充能环的随机数取 animations.visual_rng（DAT_004a6d88）
    void create(AnmManager& animations, AnmFile* effect_anm, AnmFile* bullet_anm);
    // 0x4188d0：按 id 表杀 VM 并清表
    void reset();

    // 更新链 0x1f（0x418ab0）：清掉已消亡的 id
    void sweep();
    // 0x40e6c0：游标后移找空槽（原 id 为 0 或其 VM 已不存在）；满则 -1
    i32 allocate_slot();
    // 0x41aa00：登记 VM id，返回 slot | 0x80000000（0 = 表满）
    u32 register_id(u32 id);
    // 0x418af0：kind 种类；host 非空（ins_508）时挂到该 VM，否则按表生成新 VM；返回 VM id
    u32 attach(i32 kind, AnmVm* host);
    // 0x40e730（ECL 334）：生成 kind 并登记
    u32 spawn(i32 kind, const Vec3& pos);

    // AnmVmHooks
    int update(AnmVm& vm) override;
    void deleted(AnmVm& vm) override;
    void interrupted(AnmVm& vm, i32 value) override;
    void attach_effect(i32 kind, AnmVm& vm) override { attach(kind, &vm); }

    AnmFile* effect_anm = nullptr;   // +0x0c
    AnmFile* bullet_anm = nullptr;   // +0x10
    u32 cursor = 0;                  // +0x18
    u32 ids[kIdSlots] = {};          // +0x1c

private:
    // 0x405670 分配的 0x193c 字节缓冲：50 帧 × 4 颗飞向中心的光点
    struct Ring {
        u32 ids[50][4] = {};         // +0x000
        Vec3 pos[200];               // +0x320 贝塞尔终点（第二段起点）
        Vec3 dir[200];               // +0xc80 第二段起始切线
        i32 state[200] = {};         // +0x15e0 0 未初始化 / 1 飞向环 / 2 飞向中心
        Vec3 target;                 // +0x1900 环心：entity_offset + polar($F(10035) + rotation.z, 150)
        Vec3 source;                 // +0x190c 发射点：entity_offset + polar(rotation.z, 300)
        Vec3 center;                 // +0x1918 entity_offset
        Timer timer;                 // +0x1928
    };
    void ring_init(AnmVm& vm);                 // 0x405670
    int ring_update(AnmVm& vm);               // 0x405700
    void ring_deleted(AnmVm& vm);              // 0x405ed0
    void ring_interrupted(AnmVm& vm, i32 v);   // 0x405f20

    AnmManager* animations_ = nullptr;
};

}  // namespace th16
