// th16 StageManager——原版 DAT_004a6d9c / DAT_004a6da0（0x66f0 字节，0x409db0 创建）。
//
// 锚点：
//   0x4097c0  构造：装 STD（0x40ac30）、相机取全局 0x4c168c 默认值、注册更新链 0x11 / 绘制链 3 与 6
//   0x40add0  GameManager 首帧：对象 flags=1，按遍历顺序给每个四边形建面 VM（0x407b20 + 0x45f980）
//   0x409e50  更新：清抖动位移、facing 归一化、0x40aed0 面 VM 推进、0x40b3b0 脚本、相机写回全局、层精灵 VM 推进
//   0x40b3b0  STD 脚本解释（ins 0..20），之后推进四组插值与 rockingMode 摄像机晃动
//   0x409f90  绘制（优先级 3）：相机 → 雾 → 清场地矩形 → 逐层 0x40af70（层精灵 0x40c280 + 实例四边形）
//   0x40a410  绘制（优先级 6）：以关卡相机绘制 ANM 层 0x20 / 0x21
//
// 相机结构（0x12c 字节，与全局 0x4c168c 同布局）按 dword 下标：
//   0 pos  3 facing  6 up  9 facing_norm  0xc right  0xf pos_offset  0x12 facing_offset  0x15 fov
//   0x18 view[16]  0x28 proj[16]  0x38 viewport{x,y,w,h,minz,maxz}  0x3e kind  0x3f shift{x,y}
//   0x41 pos_delta  0x44 fog_near  0x45 fog_far  0x46 fog_rgba_f[4]（B,G,R,A）  0x4a fog_color
//
// 原版把关卡画进 640×480 的场地渲染目标（3D 相机视口 (116,4,408,472)，中心 (320,240)），再由
// front.anm 精灵复制到屏幕场地 (32,16,384,448)。后端尚无渲染目标：本实现先把投影结果整体左移 96
// 像素并裁剪到场地矩形，几何与原版逐像素一致（抖动位移 DAT_004c1788 为 0 且无畸变特效时）。
#pragma once
#include "AnmManager.hpp"
#include "AnmVm.hpp"
#include "Std.hpp"
#include "Types.hpp"

#include <functional>
#include <string>
#include <vector>

namespace th16 {

struct StageFog {
    float near_ = 0, far_ = 0;
    float c[4] = {0, 0, 0, 0};   // B,G,R,A 浮点
    u32 color = 0;               // 0xAARRGGBB
};

struct StageCamera {
    Vec3 pos;                    // 0
    Vec3 facing;                 // 3
    Vec3 up{0, 1, 0};            // 6
    Vec3 facing_norm;            // 9
    Vec3 right;                  // 0xc
    Vec3 pos_offset;             // 0xf
    Vec3 facing_offset;          // 0x12
    float fov = 0.5235988f;      // 0x15
    i32 viewport_x = 116, viewport_y = 4, viewport_w = 408, viewport_h = 472;  // 0x38（0x43cb10）
    float shift_x = 0, shift_y = 0;   // 0x3f（DAT_004c1788/8c）
    Vec3 pos_delta;              // 0x41
    StageFog fog;                // 0x44..0x4a
};

class StageManager {
public:
    static constexpr int kLayerSprites = 8;
    static constexpr float kNearZ = 30.0f;    // 0x43c940 PerspectiveFovLH zn=0x41f00000
    static constexpr float kFarZ = 8000.0f;   // zf=0x45fa0000
    static constexpr float kFieldShiftX = -96.0f;  // 场地 RT 中心 (320,240) → 屏幕场地中心 (224,240)

    StdFile std;
    StageCamera camera;
    // DAT_004c168c：0x409e50 每帧脚本与插值之后整体抄出的全局相机副本（ANM 变量 10016..10021 读它；
    // 首次更新前保持默认值——原版此处为上一关 / 上一局残留，仅影响第 0 帧的读取）
    StageCamera camera_global;
    AnmFile* anm = nullptr;      // +0x66b4
    i32 stage_number = 0;        // +0x66dc

    // 脚本状态（this+0xc 起）
    Timer timer;                 // +0x0c
    i32 ins_index = 0;           // +0x20（指令下标；原版为字节偏移）
    i32 rocking_mode = 0;        // +0x24
    Timer rocking_timer;         // +0x28
    Interp<Vec3> facing_interp;  // +0x3c
    Interp<Vec3> pos_interp;     // +0x94
    Interp<Vec3> up_interp;      // +0xec
    Interp<StageFog> fog_interp; // +0x144

    std::vector<AnmVm> face_vms;            // +0x669c（面 VM 数 × 0x5fc）
    AnmVm layer_vms[kLayerSprites];         // +0x31c
    i32 layer_ids[kLayerSprites] = {};      // +0x32fc
    float distortion_radius_sq = 0;         // +0x331c（ins 20）
    i32 distortion_kind = 0;                // +0x3340（ins 17；特效对象本身待 EffectManager）

    u32 flags = 1;               // +0x66c4：bit0 绘制启用  bit1 结束中  bit2 结束淡出  bit3 隐藏
    Timer end_timer;             // +0x66c8
    i32 frame = 0;               // +0x66e0
    i32 drawn_instances = 0;     // +0x66b8
    i32 culled_instances = 0;    // +0x66bc
    i32 drawn_quads = 0;         // +0x66c0
    u32 bg_color = 0xff000000;   // DAT_004c1b0c（ins 13）
    std::string error;

    // 0x4097c0 + 0x40ac30：解析 STD；anm 由调用方装入槽 3 + (stage & 1) 后传入
    bool load(const u8* bytes, u32 size, i32 stage);
    // 0x40add0：建面 VM 并重置脚本
    void start(AnmManager& manager);
    // 0x409e50
    void update(AnmManager& manager, float game_speed, const Rng* visual_rng = nullptr);
    // 0x40b2f0 / ins 19：对全部面 VM 与层精灵 VM 挂中断并立即推进
    void interrupt(AnmManager& manager, i32 value);
    // 0x43c940 的 eye / at：eye = pos + pos_offset，at = eye + facing_norm
    Vec3 eye() const;
    Vec3 at() const;

    // 0x40af70：按原版顺序遍历第 layer 层应绘制的 VM——先 0x40c280 的层精灵（layer_sprite = true，无雾、
    // ZENABLE 关），再逐实例 × 四边形的面 VM（已写入 entity_offset / 尺寸缩放；同一面 VM 被多个实例复用，
    // 必须在回调内立即绘制）。视锥剔除 0x40a7d0 待移植——不剔除只影响计数。
    using DrawVisitor = std::function<void(AnmVm& vm, bool layer_sprite)>;
    void draw_layer(int layer, const DrawVisitor& visit);

private:
    void run_script(AnmManager& manager, float game_speed);   // 0x40b3b0 前半
    void apply_rocking(float game_speed);                       // 0x40b3b0 后半
    void step_faces(AnmManager& manager);                       // 0x40aed0
    void set_fog_time(i32 time, i32 mode, const StageFog& goal); // 0x409490
    StageFog step_fog(float game_speed);                        // 0x40cd10
};

}  // namespace th16
