// th13 MSG13 对话系统移植（S3 第一阶段：Stage 终了→对话→奖励→换关链路）。
//
// 二进制格式（从 0x428ae0 家族反汇编 + 实际文件字节解出，dump 证据留各行注释）：
//   文件头：u32 entry_count；随后 entry_count 个 8 字节表项
//     { u32 data_offset（文件内绝对偏移）; u32 id（th13 全部 0x100） }
//     ——运行时定位 = 原始文件镜像 + 表内偏移：0x428820:128
//       `*(u32*)(T + 4 + entry*8) + T`，T = 0x424fd0:51 存入 stagectx+0x19c
//       的整文件装载指针；字节核对 st01a{2 项: 0x14/0x334}、st07a{3 项:
//       0x1c/0x59c/0xd40}、e01/staff{1 项: 0x0c}，指令流起点 = 4+count*8。
//   指令：{ u16 time; u8 opcode; u8 arg_bytes; u8 args[arg_bytes] }
//     ——opcode = 指针+2 的字节（0x428ae0:151）；推进 = +4+arg_bytes
//       （0x428ae0:759）；时间门 = 对话钟 >= time 才执行（0x428ae0:143/761）。
//   entry 终止 = opcode 0 指令（case 0x0 返回 -1），其后 4 零字节填充
//     （staff.msg 全 12 条指令与 thmsg dump 逐条核对全中）。
//   文本参数（opcode 17）加密：FUN_0042a340 反编译直读——XOR 流
//     key=0x77，每字节后 key+=inc、inc+=0x10（u8 环绕），解到 NUL 止；
//     st01a entry0 全 14 句解出通顺 cp932 日文实证
//     （「さーて、幽霊の親玉を倒さないとね！」= 0xF5 0xCD… 解出 0x82 0xB3…）。
//
// 对话钟推进（0x4265a0:605-620）：每帧执行器跑到时间门/阻塞（返回 0）则
// 钟 +1（rate=1 路径）；执行器命中终止指令则当帧销毁对话（+0x198=0），
// ECL 519 dialogWait 下一帧放行。
//
// opcode 语义（dump 十进制 = 案值；st0N[a-d] 全 28 文件用例普查）：
//   0 终止 / 1 按角色装载立绘 bank / 2/19/20/31 BGM 族 / 4/5/6 关闭槽位
//   7 左侧说话人 / 8 右侧 / 9 双立绘变体 / 10 自动推进位翻转
//   11 等输入（arg=超时基准；Z 或自动模式计数通过）/ 12 单帧止步
//   13/14 隐左/右立绘 / 17 文本行（解密后支持 `|a,b,` 读音注记前缀）
//   22/27 Boss 战设置（0x43af70）/ 28 对话框 x,y / 29 文字属性位
//   21 ★关终了触发（case 0x15 → 0x42cd40，0x428ae0:673-675）
//   23/24 释放槽位 / 25 名牌缩放 / 26 显示位 / 30 原版 switch 无此案（空转）
#pragma once

#include "Types.hpp"

#include <string>
#include <vector>

namespace th13 {

struct MsgEntry {
    u32 offset = 0;  // 指令流文件内绝对偏移
    u32 id = 0;      // 表项第二 u32（th13 恒 0x100，语义待 oracle 核）
};

// 整文件镜像 + entry 表（不做指令级预解析：执行器按 4+arg_bytes 游走，
// 与原版 0x428ae0 同构）。
class MsgFile {
public:
    // 校验头与表项并保留字节镜像；失败时填 error 并返回 false。
    bool open(const u8* data, u32 size, std::string& error);
    u32 entry_count() const { return u32(entries_.size()); }
    const MsgEntry& entry(u32 index) const { return entries_[index]; }
    const u8* bytes() const { return bytes_.data(); }
    u32 size() const { return u32(bytes_.size()); }

private:
    std::vector<u8> bytes_;
    std::vector<MsgEntry> entries_;
};

// 0x42a340 文本解密：解到 NUL（含），返回解密后字节串。
std::string msg_decrypt_text(const u8* args, u32 arg_bytes);
// cp932 → UTF-8（表见 MsgPlayerCp932.hpp；半角片假名/ASCII 算法处理）。
// 非法序列按 U+FFFD 替换（对话文本为标准 cp932，预期不触发）。
std::string cp932_to_utf8(const u8* data, u32 size);

// 对话可视状态（渲染侧消费；字段语义对应 0x1a8 对话对象）。
struct MsgDialogState {
    bool active = false;
    bool done = false;                  // entry 终止指令已命中
    i32 entry = 0;                      // 对话对象 +0（0x428820 存 entry 号）
    i32 side = 0;                       // +0x184：0=左 1=右（opcode 7/8）
    float box_x = 192.0f, box_y = 320.0f;  // +0x194/+0x198（0x4282b0 尾默认）
    bool auto_advance = false;          // +0x178 bit0（opcode 10）
    u32 text_attr = 0;                  // +0x178 bit2-5（opcode 29）
    // 当前行块（≤2 行/块，opcode 17 逐行推进、11 通过后清；UTF-8）。
    std::vector<std::string> lines;
};

class MsgHost {
public:
    virtual ~MsgHost() = default;
    virtual void msg_bgm(i32 opcode) = 0;        // 2/19/20/31（BGM 族，案值透传）
    virtual void msg_portrait_load() = 0;        // 1（DAT_004c0aa0+角色*4）
    virtual void msg_boss_battle() = 0;          // 22/27（0x43af70）
    virtual void msg_stage_clear() = 0;          // 21 → 0x42cd40 ★
};

// 单 entry 播放器（0x428ae0 执行器 + 0x4265a0 钟的行为子集）。
// 本阶段数值语义边界：opcode 11 的自动模式阈值取 0x42b3f0 <0x14 观察值 20 帧、
// Z 快进按边沿处理——逐帧口径留结算窗 oracle 校准（docs/plans/event-contract.md
// §7.1 对话时长【待校准】同源）。
class MsgPlayer {
public:
    // 打开 entry（0x428820 param>=0 路径：建 0x1a8 对话对象 + 执行器指针）。
    bool open(MsgFile& file, u32 entry_index, MsgHost& host, std::string& error);
    // 每帧推进（对话钟 +1 的同帧先执行到期指令）。z_pressed = Z 边沿。
    // 返回值 = 是否仍在播放（终止指令命中的当帧起为 false）。
    bool step(bool z_pressed);
    bool active() const { return state_.active; }
    bool done() const { return state_.done; }
    u32 clock() const { return clock_; }
    const MsgDialogState& state() const { return state_; }

private:
    enum class Flow { Advance, Blocked, Finished };
    Flow execute(u8 opcode, const u8* args, u32 arg_bytes, bool z_pressed);
    void finish();  // 终止指令路径（0x4265a0 的当帧销毁）

    MsgFile* file_ = nullptr;
    MsgHost* host_ = nullptr;
    const u8* cursor_ = nullptr;  // 指令流游标（entry 内）
    MsgDialogState state_;
    u32 clock_ = 0;
    i32 line_index_ = 0;    // 对话对象 +0x17c（0x5f）
    i32 block_open_ = 0;    // +0x180（0x60）
    i32 wait_frames_ = 0;   // opcode 11 重入计数（自动模式阈值用）
    bool frame_pause_ = false;  // opcode 12（+0x174 置 1 → 本帧止步）
};

}  // namespace th13
