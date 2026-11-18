// th16 Gui——原版 DAT_004a6dcc（0x2e4 字节；0x427ca0 创建 → 0x4268c0 / 0x426b00 / 0x426c10；更新链 0x20 → 0x427cf0；
// 绘制链 0x33 / 0x30）。本阶段接入影响游戏逻辑的部分：对话（Msg）与其带出的 VM、ECL 518 / 519 / 554 入口。
// HUD（残机 / 炸弹 / Boss 血条 / 计时数字：0x426d70 建 VM、0x42c280 / 0x42c390 / 0x42c4f0 等）在渲染阶段补齐。
//
//   0x426c10  装 logo ANM（关卡表 +0x24 → 槽 6）；按机体读 msg 文件（关卡表 +0x14[shot + shot_aux]）到 +0x1cc；
//             +0x154 计时复位；+0x170 = DAT_004a57b0；+0x1d0 / +0x1d8 = -1
//   0x429ff0  对话开始（ECL 518）：entry ≥ 0 → 删旧 Msg，new Msg(0x1c8) 以 msg[4 + entry×8] 为脚本起点；
//             -1 / -3 为结局入口、-2 为过关（0x42e150）/ 续关（0x43f350）——后续阶段接入
//   0x427cf0  每帧（Msg 部分）：0x42a1d0 返回 0 → Msg+0x4 年龄计时推进；非 0（op 0 结束）→ 0x4264a0 删 VM、释放
//   0x42c240  ECL 554：logo ANM 脚本 0（Boss 登场标题；练习 / 符卡练习模式不建）
//   0x42c480  Boss 名牌 +0xd8：spell_id < 0x29 取关卡表块 1 序号，否则块 0；front.anm 脚本 序号 + 0xa4
//
// Msg（0x429b20 构造 / 0x42a1d0 解释 / 0x4264a0 析构）字段见 Msg 结构体；指令集按 0x42a1d0 的 switch 编号，
// 文字经 0x42bbe0 解密（key 0x77，步长 7 起每字节 +0x10）。文字贴图（0x46d990 AsciiManager 画进 VM 纹理）
// 不改变逻辑状态，这里把解密后的文本与颜色记入 text_requests 供宿主渲染。
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "AnmVm.hpp"
#include "Types.hpp"

namespace th16 {

class AnmManager;
class AnmRenderer;
class BulletManager;
class EnemyManager;
struct GameGlobals;
struct InputState;
class Player;
struct PlayerWorld;
struct StageEntry;
struct AnmFile;

// 对话脚本（msg 文件）：entry 表 {u32 count; {u32 offset; u32 pad}[count]}，指令 {u16 time; u8 op; u8 size; u8 data[size]}
struct MsgInstruction {
    u16 time;
    u8 opcode;
    u8 size;
    u8 data[1];
};

struct Msg {
    static constexpr u32 kSkippable = 0x1;      // +0x190 bit0：op 10——本段可被长按射击快进
    static constexpr u32 kTextShadow = 0x2;     // bit1：op 26 置、op 7/8/9/32 清——文字 VM 用第二套样式
    static constexpr u32 kStyleShift = 2;       // bit2..5：op 29 文字框样式（front.anm 0xe4 + side + style×2）
    static constexpr u32 kStyleMask = 0x3c;
    static constexpr u32 kShotPressed = 0x40;   // bit6：按过射击 / 跳过键（与 kSkippable 同置才允许快进）

    i32 entry = 0;              // +0x00
    Timer age;                  // +0x04：Gui 每帧推进（0x427cf0）
    Timer timer;                // +0x18：脚本时刻（op 11 等待期间不推进）
    Timer wait;                 // +0x2c：op 11 倒计时
    u32 portrait_vm = 0;        // +0x40：自机 / 对方立绘（op 1）
    u32 face_vms[4]{};          // +0x44..+0x50：Boss 立绘（op 2 / 31，关卡表 Boss 块）
    u32 frame_vm = 0;           // +0x54（th16 无创建路径，析构 / 中断照旧处理）
    u32 text_vms[4]{};          // +0x58 / +0x5c：text.anm 0 两行；+0x60 / +0x64：text.anm 1 两行（右侧 / 阴影）
    u32 name_vm = 0;            // +0x68：Boss 名牌（op 20）
    u32 box_vm = 0;             // +0x6c：文字框（front.anm 0xe4 + 下标，0x42ba30）
    const u8* pc = nullptr;     // +0x158
    Vec3 anchors[4] = {{16, 16, 0}, {16, 16, 0}, {16, 16, 0}, {16, 16, 0}};  // +0x15c..+0x188：op 9 文字 VM 基点
    i32 ecl_release = 0;        // +0x18c：op 12 置 1、每帧 -1；ECL 519 在其为 0 时等待
    u32 flags = 0;              // +0x190
    i32 lines_written = 0;      // +0x194：本段已写入的文字行数
    i32 lines_cleared = 0;      // +0x198：本段已清空文字 VM
    i32 side = 0;               // +0x19c：0 = 左（自机）/ 1 = 右（Boss）
    u32 colors[4]{};            // +0x1a0..+0x1ac：各侧文字颜色（0 = 默认白）
    Vec3 text_pos{384, 640, 0}; // +0x1b0：op 28（参数 ×2）
    float text_width = 320;     // +0x1bc：文字框宽（按最长行 max((len×8 & ~0xf) − 0x1c) × 2）
    i32 unknown_1c0 = 0;        // +0x1c0
    i32 box_index = 0;          // +0x1c4：当前文字框脚本下标（0x42ba30 存，跟随子 VM 0xb4 + 下标）

    // 宿主渲染用：0x46d990 把文字画进 VM 纹理的请求（逻辑无副作用）
    struct TextRequest { u32 vm_id; std::string text; u32 color; u32 shadow_color; i32 font; i32 x; i32 y; };
    std::vector<TextRequest> text_requests;

    const MsgInstruction* ins() const noexcept { return reinterpret_cast<const MsgInstruction*>(pc); }
    i32 iarg(int i) const noexcept;
    float farg(int i) const noexcept;
};

class Gui {
public:
    static constexpr int kLogoSlot = 6;  // 0x426c10：关卡 logo ANM 装入槽 6（oracle banks[6] = stNNlogo.anm）

    AnmFile* front = nullptr;        // +0x2d8：front.anm（槽 5）
    AnmFile* logo = nullptr;         // +0x178：关卡 logo ANM（槽 6）
    std::vector<u8> msg_data;        // +0x1cc：对话脚本文件
    std::unique_ptr<Msg> dialogue;   // +0x1c8
    u32 boss_name_vm = 0;            // +0xd8（0x42c480）
    u32 vm_110 = 0;                  // +0x110：op 35（0x426780）front.anm 0x3c
    u32 boss_title_vm = 0;           // 0x42c240：logo 脚本 0（原版不保存 id）
    u32 flags_1ac = 0;               // +0x1ac
    Timer timer_154;                 // +0x154
    i32 boss_timer_display = -1;     // +0x1d0
    i32 boss_timer_shown = -1;       // +0x1d8

    // 宿主绑定
    AnmManager* animations = nullptr;
    const AnmRenderer* geometry = nullptr;   // 0x406a70 / 0x406c40（文字框子 VM 的屏幕位置）
    EnemyManager* enemies = nullptr;         // DAT_004a6dc0（ANM 表 / 全灭 / Boss 槽）
    BulletManager* bullets = nullptr;        // DAT_004a6dac（0x416f40 全屏消弹）
    Player* player = nullptr;                // DAT_004a6ef8（立绘 ANM 文件）
    PlayerWorld* world = nullptr;            // dialogue 标志的宿主镜像（DAT_004a6dcc->0x1c8 != 0）
    GameGlobals* globals = nullptr;
    const InputState* input = nullptr;       // DAT_004a52c8 / DAT_004a52d4 / DAT_004a5244[]
    const StageEntry* entry = nullptr;       // DAT_004a6f18
    bool replay_playback = false;            // GameManager+0xac != 0
    std::vector<i32> sfx_queue;              // 0x45e150：op 11 按键推进 0
    // 0x43c3f0(1, 曲目) / 0x43c470：op 19 切 Boss 曲、op 22 / 27 淡出（Sound 接入前供宿主取走）
    // kind：0 淡出（0x43c470）/ 1 Boss 曲（0x43c3f0）/ 2 直接切曲（0x45ed00(2, 序号)，ECL 518 -1 / -3）
    struct BgmEvent { i32 kind; i32 track; };
    std::vector<BgmEvent> bgm_events;
    std::string current_bgm;                 // DAT_004dc0f0：当前曲文件名（Sound 接入前仅由本类的切曲事件维护）
    std::string error;

    // 0x426c10（msg 部分）：按机体选 msg 文件；logo ANM 由 GameSession 装入槽 6 后传入
    bool init(AnmManager& anm, AnmFile* front_anm, AnmFile* logo_anm, const StageEntry& stage, std::vector<u8> msg_bytes);
    // 0x429ff0：entry ≥ 0 开始对话；-1 / -3 切曲 + logo 标题；-2（过关 / 续关）当前记错误返回 false
    bool start_dialogue(i32 entry_index);
    // ECL 519（0x41dcb0 case 0x40）：对话存在且 ecl_release == 0 时需等待
    bool ecl_waits() const noexcept { return dialogue != nullptr && dialogue->ecl_release == 0; }
    // 0x42c240：ECL 554 Boss 登场标题
    void show_boss_title();
    // 0x42c480
    void ensure_boss_name();
    // 0x427cf0（Msg 部分）
    void update(float game_speed);
    // 0x427a20 / 0x4264a0：离开游戏时删掉全部对话 VM
    void shutdown();

private:
    // 0x429b20
    void create_dialogue(i32 entry_index, const u8* script);
    // 0x4264a0
    void destroy_dialogue();
    // 0x42a1d0：返回 true 表示脚本结束（op 0）
    bool run_dialogue(float game_speed);
    void execute(Msg& m, const MsgInstruction& ins);
    // 0x42ba30 / 0x42bb30
    void create_text_box(Msg& m, i32 index);
    void resize_text_box(Msg& m, i32 index);
    // 0x46f3e0：按 id 设 entity_offset（找不到则清 id）
    void set_offset(u32& id, const Vec3& pos);
    // 0x46f2e0：找 VM，找不到则清 id
    AnmVm* find_or_clear(u32& id);
    // 0x46d990 请求
    void draw_text(Msg& m, u32& id, const char* text, u32 color, u32 shadow, i32 font, i32 x, i32 y);
    void clear_text_vms(Msg& m);
    void kill_vm(u32& id);
    void sync_world() noexcept;
    AnmFile* enemy_anm(i32 index) const;
    static std::string decrypt_text(const u8* src);  // 0x42bbe0
};

}  // namespace th16
