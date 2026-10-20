// GameHud 文本 pass：hi-score/score、残机/灵击碎片、灵力（th15 GameHudDraw.cpp
// 的增量逼近与两段字号照搬；灵力按 th13 x.xx /4.00，power_step=100）。
#include "GameHud.hpp"

#include <algorithm>
#include <cstdio>

namespace th13 {

namespace {
template <typename... T>
std::string format(const char* pattern, T... values) {
    char text[256];
    std::snprintf(text, sizeof(text), pattern, values...);
    return text;
}

u32 alpha(u32 color, u32 source) { return (color & 0xffffff) | (source & 0xff000000); }
}  // namespace

std::string grouped_number(u32 value) {
    if (value < 1000) return format("%d", signed_bits(value));
    if (value < 1000000) return format("%d,%.3d", value / 1000, value % 1000);
    if (value < 1000000000)
        return format("%d,%.3d,%.3d", value / 1000000, (value / 1000) % 1000, value % 1000);
    return format("%d,%.3d,%.3d,%.3d", value / 1000000000, (value / 1000000) % 1000,
                  (value / 1000) % 1000, value % 1000);
}

std::string score_number(u32 value, i32 last_digit) {
    if (value < 100) return format("%d", signed_bits(value * 10 + u32(last_digit)));
    if (value < 100000) return format("%d,%.2d%d", value / 100, value % 100, last_digit);
    if (value < 100000000)
        return format("%d,%.3d,%.2d%d", (value / 100000) % 1000, (value / 100) % 1000,
                      value % 100, last_digit);
    return format("%d,%.3d,%.3d,%.2d%d", (value / 100000000) % 1000, (value / 100000) % 1000,
                  (value / 100) % 1000, value % 100, last_digit);
}

void GameHud::update_score() {
    if (score.score != displayed_score) {
        u32 change = u32(wrapping_add(score.score, -displayed_score)) >> 5;
        change = std::min(578910u, std::max(1u, change));
        if (score_increment < i32(change)) score_increment = i32(change);
        const i32 difference = wrapping_add(score.score, -displayed_score);
        if (difference < score_increment) score_increment = difference;
        displayed_score = wrapping_add(displayed_score, score_increment);
        if (score.score <= displayed_score) score_increment = 0;
    }
    if (progress.high_score < displayed_score) {
        progress.high_score_extra = progress.continues;
        player.mode_flags |= 4;
        progress.high_score = displayed_score;
    }
}

bool GameHud::draw(const HudDrawContext& context, HudDrawServices& services) {
    HudTextStyle style;
    const auto text = [&](float x, float y, const std::string& value) {
        if (services.hud_text({{x, y, 0}, style, value})) return true;
        error = "HUD text draw service failed";
        return false;
    };
    const auto* root_vm = animations.registry.find(life_icons[0]);
    if (!root_vm) {
        error = "HUD life icon animation unavailable";
        return false;
    }
    (void)context;
    // hi-score / score：右对齐，尾位为 continues / hi_score_extra。
    style.color = alpha(0xff808080, root_vm->visual.color);
    style.font = 4;
    style.alignment = 2;
    if (!text(620, 42, score_number(u32(progress.high_score), progress.high_score_extra)))
        return false;
    style.color = alpha(0xffffffff, root_vm->visual.color);
    if (!text(620, 64, score_number(u32(displayed_score), progress.continues))) return false;
    // 残机/灵击碎片：右对齐小字号。
    style.alignment = style.vertical_alignment = 1;
    style.scale = {.6f, .6f};
    if (!text(576, 120, format("%3d", player.life_pieces))) return false;
    if (!text(597, 120, format("/%d", 5))) return false;
    if (!text(576, 158, format("%3d", player.bomb_pieces))) return false;
    if (!text(597, 158, format("/%d", 5))) return false;
    // 灵力：x.xx /4.00 两段字号（整数段大字号、小数段小字号）。
    if (!player.power_step) {
        error = "HUD power step is zero";
        return false;
    }
    style.color = alpha(0xffff8030, root_vm->visual.color);
    style.scale = {1, 1};
    if (!text(540, 182, format("%d.", player.power / player.power_step))) return false;
    style.scale = {.6f, .6f};
    if (!text(560, 189,
              format("%.2d", wrapping_mul(player.power % player.power_step, 100) / player.power_step)))
        return false;
    style.scale = {1, 1};
    if (!text(574, 182, format("/%d.", player.max_power / player.power_step))) return false;
    style.scale = {.6f, .6f};
    if (!text(606, 189, "00")) return false;
    return true;
}

}  // namespace th13
