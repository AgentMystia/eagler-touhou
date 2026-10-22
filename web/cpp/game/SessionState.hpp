// 对局进度归属（最小集，docs/plans/ui-base.md §三）：
// 进度属于 run，穿越场景工厂；区别于逐帧输入、显示状态与存档记录。
#pragma once

#include "Types.hpp"

namespace th13 {

struct SessionState {
    i32 stage = 1, stage_frame = 0, character = 0;
    i32 difficulty = 0, continues = 0;
    i32 high_score = 0, high_score_extra = 0;
    i32 configured_lives = 3;
    i32 starting_stage = 1;
    float rate = 1;
    u32 scene_flags = 0;
    bool new_run = true;
    // ---- S3 对局终了链路（results-overlay-anatomy.md / results-flow.md）----
    // DAT_004be830：bit4-5 = mode（0=故事 1=Extra 2=符卡练习 3=Overdrive 同 2 类）、
    // bit6 = 演示回放；0x42cd40 以 (flags>>4)&3 取 mode、&0x30==0x20 门符卡练习。
    u32 run_mode_flags = 0;
    // GameInf+0x74 模式参数（0=常规 1=练习）：非 0 时关终改走结算A（0x43e5a0）。
    i32 practice_mode = 0;
    // DAT_004be808（「白点」计数，>599 → 好结局资格）与 DAT_004be83c（好结局
    // 资格计数，残≥3 时置 3；结局选择的门见 results-flow.md §4.2）。
    i32 ending_points = 0;
    i32 good_endings = 0;
    // GameInf+0x2ac：本次关终 clear bonus 累计（0x42abf0 覆盖写、六面/Extra
    // 再 += 同式原值；HUD 通关演出消费——显示 TODO）。
    i32 stage_clear_bonus = 0;
    // score 数据块 +0x7fc 起的关卡通过位（内存态；ScoreFile 落档属后期阶段）。
    // 位序 = stage + difficulty*8（0x42cd40:100 的字节写位同式）。
    u32 stage_cleared_bits = 0;
};

// HUD 侧玩家资源视图（battle 未接入前的持有者；完整 PlayerLifeSession
// 随 Stage 分期落地，字段名与 th15 对齐便于迁移）。
// 碎片阈值取 front.anm script30/38 的 interrupt 标签布局（2=满/3=空/7+n=碎片，
// n∈[0,5] ⇒ 5 碎片一整），oracle 对照后修正。
struct HudPlayerState {
    i32 extra_lives = 2, life_pieces = 0;
    i32 bombs = 2, bomb_pieces = 0;
    i32 power = 100, max_power = 400, power_step = 100;
    i32 deaths = 0;
    u32 mode_flags = 0;
};

// HUD 分数视图：score 为逻辑分（尾位由 continues/hi_score_extra 补显示）。
struct HudScoreState {
    i32 score = 0;
    i32 life_piece_tier = 0;
};

}  // namespace th13
