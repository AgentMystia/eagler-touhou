// th16 回放记录——原版 ReplayManager 记录侧（0x447fd0 逐帧写入、0x448eb0 开段、0x448400 落盘）。
//
// 锚点：
//   0x447fd0  每帧：自动低速（DAT_004c12cc bit9）后写 held/pressed/released 各 u16，每 30 帧写 FPS 字节
//   0x448eb0  关卡起点：清帧计数、为当前关分配输入块、抄 GameManager 快照进段头 0x294
//   0x448400  组装用户块 0xa0 + 各段 → LZSS(0x457b20) → 双重 0x402220 → 写 replay/th16_XX.rpy + USER 说明块
//
// GameSession 在非回放局把记录链接到本类；GameMenu::ReplayStore::save 调 write_slot。
#pragma once
#include "GameMenu.hpp"
#include "ReplayFile.hpp"
#include "Types.hpp"

#include <string>
#include <vector>

namespace th16 {

struct GameGlobals;

struct ReplayRecording {
    static constexpr u32 kMaxInputBytes = 2u * 1024u * 1024u;

    std::vector<u8> user;                         // 0xa0 用户块（开局填机体/难度/季节；保存时写名字/分数）
    std::vector<ReplayStageSegment> stages;       // 已封口的关卡段
    ReplayStageSegment current;                   // 进行中的段（frames 随 tick 增长）
    bool stage_open = false;
    bool sealed = false;
    i32 timestamp = 0;                            // 用户块 +0xc（time32）
    std::string error;

    void reset();
    // 开局：填用户块中的机体 / 难度 / 季节 / 模式标志；时间戳由宿主提供
    void begin_run(const GameGlobals& globals, i32 time32);
    // 0x448eb0：开始新关卡段（封口上一关）；seed = game_rng 种子低 16 位
    void begin_stage(i16 stage, u16 rng_seed);
    // 0x447fd0：记录一帧（held/pressed/released 低 16 位；fps 每 30 帧采样一次）
    bool tick(u16 held, u16 pressed, u16 released, u8 fps_byte);
    // 封口当前关（换关 / 结束前）；可选追加 0xffff 终止三元组（0x448400 非 spell 路径）
    void close_stage(bool append_terminator);
    // 0x448400：写出 th16_XX.rpy（directory 下优先 replay/ 子目录；失败时写 directory 根）
    bool write_slot(const char* directory, int slot, const char* player_name, i32 score, float slow_rate);
    // 结构化 → 文件字节（不含路径）；成功时 out 含头 + 密文 + USER 说明块
    bool encode(std::vector<u8>& out, const char* player_name, i32 score, float slow_rate);

    // 供菜单列表：从磁盘槽读头
    static bool slot_exists(const char* directory, int slot);
    static bool slot_summary(const char* directory, int slot, ReplaySummary& out);
};

}  // namespace th16
