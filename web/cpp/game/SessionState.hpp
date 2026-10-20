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
