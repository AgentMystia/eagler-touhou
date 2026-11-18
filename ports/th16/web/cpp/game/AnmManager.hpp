// th16 ANM 管理器——原版 AnmManager（DAT_004c0f48，0x46a3a0 构造）的行为镜像。
//
// 职责与原版地址：
//   VM 池 0x1fff 个（+0xec 起，每个 0x614）与 LIFO 空闲链：0x46f600 取 / 0x46eab0 还
//   世界链表 +0xdc/+0xe0、UI 链表 +0xe4/+0xe8：0x46e7d0 / 0x46e890 / 0x46e940 / 0x46ea00 插入
//   bank 槽 +0x184f4f0（31 个 AnmFile*）：0x46cf80 装载 → 0x46d1c0 逐 entry 建精灵表 / 模板
//   每帧：0x46e340（世界链）/ 0x46e490（UI 链）逐 VM 调 0x45f980 并按 layer 串绘制链 +0x1c6fc30
//   id 分配：+0x1c7fd84 代数计数（19 位，非零）<<13 | 池下标
#pragma once
#include <memory>
#include "AnmResource.hpp"
#include "AnmVm.hpp"
#include "Rng.hpp"

namespace th16 {

// 运行时精灵记录（0x44 字节，0x46d3b0 填充 + 0x46d8a0 归一化）。
struct AnmRuntimeSprite {
    i32 file_slot = 0;       // +0x00
    i32 entry_index = 0;     // +0x04
    i32 key = 0;             // +0x08  slot<<8 | entry
    float x0 = 0, y0 = 0;    // +0x0c  纹理像素矩形（已乘 ratio）
    float x1 = 0, y1 = 0;    // +0x14
    float tex_h = 0;         // +0x1c
    float tex_w = 0;         // +0x20
    float u0 = 0, v0 = 0;    // +0x24
    float u1 = 0, v1 = 0;    // +0x2c
    float height = 0;        // +0x34  (y1-y0)/ratio_y
    float width = 0;         // +0x38  (x1-x0)/ratio_x
    float ratio_x = 1;       // +0x3c  texW / entry.w
    float ratio_y = 1;       // +0x40  texH / entry.h
};
static_assert(sizeof(AnmRuntimeSprite) == 0x44, "sprite record");

// 已装载的 ANM 文件（原版 0x13c 结构：+0 槽号 +0x10c 模板 +0x11c 精灵表 +0x120 脚本指针表）。
struct AnmFile {
    i32 slot = 0;
    const AnmResource* resource = nullptr;
    std::vector<AnmRuntimeSprite> sprites;         // 全局精灵编号（跨 entry 累加）
    std::vector<const AnmScript*> scripts;         // 全局脚本编号
    std::vector<AnmVm> templates;                  // 每脚本一份，装载末步预跑 "-1:" 块
    std::vector<u32> texture_width, texture_height;// 每 entry 实际纹理尺寸（D3D GetLevelDesc）
};

// VM 回调表（update DAT_004919e8 / delete DAT_00491b58 / interrupt DAT_00491b0c）：原版按 VM 的回调下标
// （0x5c4 / 0x5cc / 0x5d0）查全局函数表；移植版由宿主子系统（EffectManager）实现本接口并挂到 AnmManager::hooks。
struct AnmVmHooks {
    virtual ~AnmVmHooks() = default;
    // 0x45f980 慢放处理之后、指令执行之前；返回非零 → 本帧结束并删除 VM
    virtual int update(AnmVm& vm) = 0;
    // 0x46eab0 回收前
    virtual void deleted(AnmVm& vm) = 0;
    // 0x46f0b0 / 0x46f130 置 pending_interrupt 之前（EDX = 中断值）
    virtual void interrupted(AnmVm& vm, i32 value) = 0;
    // ins_508 → 0x418af0(kind, vm)
    virtual void attach_effect(i32 kind, AnmVm& vm) = 0;
};

class AnmManager {
public:
    static constexpr u32 kPoolSize = 0x1fff;
    static constexpr int kLayerCount = 0x24;
    static constexpr int kBankSlots = 31;
    static constexpr int kAsciiSlot = 2;   // AsciiManager（DAT_004a6d98，0x407cb0）把 ascii*.anm 装入槽 2

    AnmManager();

    // 0x46cf80 + 0x46d1c0：把资源装进槽位，建精灵表、脚本表并预跑模板。texture 尺寸由调用方给出
    // （浏览器端为 GraphicsDevice 实际创建的纹理尺寸，离线 harness 为 entry 声明尺寸）。
    AnmFile* load(int slot, const AnmResource& resource);
    AnmFile* file(int slot) const { return slot >= 0 && slot < kBankSlots ? banks_[slot].get() : nullptr; }
    // 0x46f270：标记该槽全部世界/UI VM 待删（flags_hi |= 0x20）；0x46d770 的前半。调用方随后 tick 回收，再 unload。
    void kill_file(int slot);
    // 释放 bank 槽（需已 kill + tick，或槽上无活跃 VM）
    void unload(int slot);

    // 0x406380：生成脚本 VM（挂到世界链表尾），返回 id；out_vm 可取 VM 指针
    u32 spawn(AnmFile& file, int script, int layer, AnmVm** out_vm = nullptr);
    // 0x40e5c0：同 0x406380，但在首帧 run 前设 entity_offset 与 rotation.z
    u32 spawn_effect(AnmFile& file, int script, int layer, const Vec3& pos, float rotation_z = 0.0f);
    // 0x426160：同上但挂到世界链表头、creation_flags = 2（自机满火力附加 VM 等）
    u32 spawn_front(AnmFile& file, int script, int layer, AnmVm** out_vm = nullptr);
    // 以上三者及 0x41dcb0 op 307/315/338、0x41bb50 朝向切换等内联展开的公共形：
    // 池取 → 模板复制 → flags_hi |= 0x400 → layer（≥0 才设；<0x18 时 flags_hi=(hi&~0x80000)|0x40400）
    // → entity_offset / rotation.z → 首帧 run → creation_flags → 插入世界链表（front/back）
    u32 spawn_vm(AnmFile& file, int script, int layer, const Vec3& offset, float rotation_z, i32 creation_flags,
                 bool front, AnmVm** out_vm = nullptr);
    // 0x46f1c0：按 id 标记 VM 待删（flags_hi = (flags_hi & ~0x40) | 0x20），并递归 0x46f220 其子树；0x4000000 位保护
    void kill(u32 id);
    static void kill_tree(AnmVm& vm);  // 0x46f220
    // 0x46f340 / 0x46f3b0：整棵子树清 flags_lo bit1（敌机 flags & 0x20 时绑定即隐藏）
    static void hide_tree(AnmVm& vm);
    // 0x46f300 / 0x46f380：整棵子树置 flags_lo bit1（ECL 503 清掉 0x20 后对全部 VM 槽调用）
    static void show_tree(AnmVm& vm);
    // 0x46e340：世界链表逐帧推进；0x46e490：UI 链表
    void tick_world();
    void tick_ui();
    // 0x46efa0：按 id 找 VM
    AnmVm* find(u32 id);
    // 0x46f510：在 vm 的子树（+0x588 子表，递归）里找第 nth 个 template_script == script 的 VM；script == -1 匹配任意
    static AnmVm* find_child(AnmVm& vm, i32 script, i32 nth);
    // 0x46f0b0：给 id 对应 VM 及其直接子 VM 挂 pending_interrupt（不立即执行）
    void interrupt(u32 id, i32 value);
    // 0x46f130：同上，但随即对每个 VM 调一次 run（0x45f980）
    void interrupt_now(u32 id, i32 value);

    // 0x45f980：单 VM 推进；返回非零表示应删除
    int run(AnmVm& vm);
    // 0x4090f0：VM 复位（保留 entity_offset、池下标、layer；宿主内嵌 VM 重绑脚本前调用）
    static void reset_vm(AnmVm& vm);
    // 0x406340：沿 root 链（遇 0x10000 分离位即止）取最顶层祖先的慢放系数
    static float root_slowdown(const AnmVm& vm) noexcept;
    // 0x407b20：把脚本模板复制到调用方自备的 VM（StageManager 的面 VM / 层精灵不入池）
    void init_from_template(AnmFile& file, AnmVm& vm, int script);
    // 0x45f160：不经模板，把宿主自备的 VM（敌弹 / 激光，内嵌于宿主对象）绑到 file 的 script 并立即跑首帧
    void init_script(AnmFile& file, AnmVm& vm, int script);

    // DAT_00491b1c：ins_300 / ins_301 精灵号重映射钩子（1 = 敌弹 0x417140，2 = 激光 0x431fa0，3 = 0x43a840：经宿主 +0x5ec → +0x5d0 + 0x20c 取表，属激光数据而非道具；ItemManager 不挂此钩子）
    static constexpr u32 kSpriteMapperCount = 4;
    using SpriteMapper = i32 (*)(AnmVm& vm, i32 index);
    SpriteMapper sprite_mappers[kSpriteMapperCount] = {};
    AnmVmHooks* hooks = nullptr;

    AnmListNode* world_head() const { return world_head_; }
    AnmListNode* ui_head() const { return ui_head_; }
    AnmVm* layer_chain(int layer) const { return layer_heads_[layer]; }
    const AnmVm* pool_vm(u32 index) const { return &pool_[index]; }

    // 全局状态（原版为独立全局变量，oracle 中亦为零/默认）
    float game_speed = 1.0f;          // DAT_004a5788
    const AnmVm* running_vm = nullptr;  // 正在 run 的 VM（0x45f980 入口；RNG 调试钩子归因用）
    Rng game_rng;                     // DAT_004a6d80
    Rng visual_rng;                   // DAT_004a6d88
    // DAT_004c168c 全局相机副本（StageManager::camera_global）：ANM 变量 10016..10018 = pos + pos_offset、
    // 10019..10021 = facing_norm；未绑定（标题 / 无关卡）时读 0
    const Vec3* camera_pos = nullptr;
    const Vec3* camera_pos_offset = nullptr;
    const Vec3* camera_facing_norm = nullptr;
    Vec3 entity_offset_delta;         // DAT_004c1790..98（= 全局相机副本的 pos_delta，StageManager::update 每帧写入）
    bool paused = false;              // DAT_004a6dd4->0x88 & 2
    u32 spawn_count = 0;              // +0x134（文件计数）
    u32 world_count = 0;              // +0xd8
    u32 render_target_width = 640;    // DAT_004d9d2c（0x45b280：640 × DAT_004d9d34 窗口缩放）
    u32 render_target_height = 480;   // DAT_004d9d30

private:
    friend struct AnmInterpreter;
    AnmVm* allocate();                       // 0x46f600
    void release(AnmVm& vm);                 // 0x46eab0
    void mark_deleted(AnmVm& vm, AnmListNode& list);  // 0x46e660
    u32 insert_world_back(AnmVm& vm);        // 0x46e7d0
    u32 insert_world_front(AnmVm& vm);       // 0x46e890
    u32 insert_ui_back(AnmVm& vm);           // 0x46e940
    u32 insert_ui_front(AnmVm& vm);          // 0x46ea00
    u32 next_id(AnmVm& vm);
    AnmVm* create_child(AnmFile& file, int script, AnmVm& parent, u32 list_flags, u32* out_id);  // 0x46ed60
    AnmVm* create_detached(AnmFile& file, int script, AnmVm& parent, u32* out_id);               // 0x46eea0
    void tick_list(AnmListNode*& head, bool ui);

    std::vector<AnmVm> pool_;
    std::vector<u8> pool_in_use_;
    std::vector<u32> free_stack_;
    std::unique_ptr<AnmFile> banks_[kBankSlots];
    AnmListNode* world_head_ = nullptr;
    AnmListNode* world_tail_ = nullptr;
    AnmListNode* ui_head_ = nullptr;
    AnmListNode* ui_tail_ = nullptr;
    AnmVm* layer_heads_[kLayerCount] = {};
    u32 id_counter_ = 0;
};

}  // namespace th16
