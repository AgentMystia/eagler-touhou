// th16 结局——原版 DAT_004a6dbc（0x419640 创建 / 0x4191f0 装载 / 更新链 0x23 / 绘制链 0x43）。
//
// 锚点：
//   0x4191f0  登记更新/绘制链；选 e0N.msg（N=ending_index+1）；成绩 +0x19f96 见过标记；ascii Loading 0x11
//   0x4197c0  结局侧 Msg 壳（text.anm 0x2e..）；剧本推进随后接
//   0x419450  析构：摘链、释放 msg / 结局 VM
//
// 本阶段：场景进出、索引/msg 装载、成绩见过位、跳过/超时回标题；完整 Msg 剧本 VM 随后接。
#pragma once
#include "Input.hpp"
#include "Types.hpp"

#include <string>
#include <vector>

namespace th16 {

struct ScoreFile;
class AnmManager;
struct AnmFile;

struct EndingScene {
    static constexpr i32 kSkipFrames = 0x78;
    static constexpr i32 kAutoFrames = 0x708;
    static constexpr const char* kMsgNames[8] = {
        "e01.msg", "e02.msg", "e03.msg", "e04.msg", "e05.msg", "e06.msg", "e07.msg", "e08.msg",
    };

    bool live = false;
    i32 timer = 0;           // +0x10 族
    i32 ending_index = 0;    // +0x18：机体组合×2（+1 = 有续关）
    u32 flags = 0;           // +0x1c：bit0 本结局未见过 / bit1 全机未见
    bool finished = false;
    std::vector<u8> msg_bytes;   // e0N.msg 载荷（原版 +0x10 指向解密缓冲）
    std::string msg_name;
    u32 line_vms[5] = {};    // 0x4197c0：text.anm 脚本 0x2e..0x32
    const u8* script_pc = nullptr;  // +0x54：剧本游标（完整解释器待接）
    std::vector<const char*> events;

    // 0x4191f0：shot+aux；continues==0 → 偶下标（true end），否则奇下标
    void begin(i32 shot, i32 shot_aux, i32 continues, ScoreFile* scores, i32 difficulty);
    // 从档案装 e0N.msg；失败时 events 记 "msg-missing"，仍可跳过回标题
    bool load_msg(const u8* bytes, u32 size, const char* name);
    // 0x4197c0：text.anm 五行 VM；index≥8 时 flags|=2
    void spawn_line_vms(AnmManager& anm, AnmFile* text_anm);
    void destroy_line_vms(AnmManager& anm);
    bool update(const InputState& input);
    void end();
};

}  // namespace th16
