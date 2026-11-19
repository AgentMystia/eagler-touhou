// th16 结局——原版 DAT_004a6dbc（0x419640 创建 / 0x4191f0 装载 / 更新链优先级 0x23）。
// 本文件先还原场景进出与跳过/超时回标题；ending*.anm 剧本与文字贴图随后接。
#pragma once
#include "Input.hpp"
#include "Types.hpp"

#include <vector>

namespace th16 {

struct EndingScene {
    static constexpr i32 kSkipFrames = 0x78;     // 可跳过前的最短停留（对齐常见 TH 结局门控量级）
    static constexpr i32 kAutoFrames = 0x708;    // 无输入自动回标题上限（与标题 demo 同量级，便于测）

    bool live = false;
    i32 timer = 0;           // +0x10 族
    i32 ending_index = 0;    // +0x18：机体组合 ×2（+1 为 true end）
    u32 flags = 0;           // +0x1c：bit0 未见过 / bit1 全机未见 等
    bool finished = false;
    std::vector<const char*> events;

    // 0x4191f0 入口参数：shot+shot_aux；has_true_end ← 无续关通关等
    void begin(i32 shot, i32 shot_aux, bool has_true_end);
    // 每帧：确认/取消在可跳过后结束；超时结束。返回 true 表示请求回标题。
    bool update(const InputState& input);
    void end();
};

}  // namespace th16
