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

    AnmListNode* world_head() const { return world_head_; }
    AnmListNode* ui_head() const { return ui_head_; }
    AnmVm* layer_chain(int layer) const { return layer_heads_[layer]; }
    const AnmVm* pool_vm(u32 index) const { return &pool_[index]; }

    // 全局状态（原版为独立全局变量，oracle 中亦为零/默认）
    float game_speed = 1.0f;          // DAT_004a5788
    Rng game_rng;                     // DAT_004a6d80
    Rng visual_rng;                   // DAT_004a6d88
    Vec3 entity_offset_delta;         // DAT_004c1790..98
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
    void init_from_template(AnmFile& file, AnmVm& vm, int script);  // 0x407b20
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
