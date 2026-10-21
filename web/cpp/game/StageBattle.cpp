// StageBattle 实现。ECL 步进与敌机/弹幕数据全部来自 StageSim；本文件只
// 补自机侧行为（近似定标，见 StageBattle.hpp 文件头）。
#include "StageBattle.hpp"

#include <cmath>

namespace th13 {

bool StageBattle::load(EclResourceProvider& provider, const char* stage_file, i32 difficulty_rank,
                       StageBattleServices& host, SessionState& progress, HudPlayerState& player,
                       HudScoreState& score) {
    host_ = &host;
    progress_ = &progress;
    player_ = &player;
    score_ = &score;
    player_x = 0.0f;
    player_y = 416.0f;
    invulnerable = kInvulnFrames;
    if (!sim.begin(provider, stage_file, difficulty_rank)) {
        error_ = sim.error();
        return false;
    }
    if (!sim.start()) {
        error_ = sim.error();
        return false;
    }
    return true;
}

void StageBattle::move_player(u32 held) {
    focus = (held & 0x08) != 0;
    float dx = 0, dy = 0;
    if (held & 0x10) dy -= 1;  // ↑
    if (held & 0x20) dy += 1;  // ↓
    if (held & 0x40) dx -= 1;  // ←
    if (held & 0x80) dx += 1;  // →
    // 斜向小键盘位并入（0x50 右上 0x60 左上 0x90 右下 0xa0 左下）
    if (held & 0x50) { dx += 1; dy -= 1; }
    if (held & 0x60) { dx -= 1; dy -= 1; }
    if (held & 0x90) { dx += 1; dy += 1; }
    if (held & 0xa0) { dx -= 1; dy += 1; }
    if (dx != 0 && dy != 0) {
        const float inverse = 0.70710678118654752f;
        dx *= inverse;
        dy *= inverse;
    }
    const float speed = focus ? kPlayerSpeedFocus : kPlayerSpeed;
    player_x += dx * speed;
    player_y += dy * speed;
    // 场内钳制（贴边留半个机身）。
    if (player_x < -176.0f) player_x = -176.0f;
    if (player_x > 176.0f) player_x = 176.0f;
    if (player_y < 32.0f) player_y = 32.0f;
    if (player_y > 432.0f) player_y = 432.0f;
}

void StageBattle::fire_shots() {
    if (fire_cooldown_ > 0) {
        --fire_cooldown_;
        return;
    }
    fire_cooldown_ = kFireInterval;
    // 近似弹型：3 发扇面（0°、±0.10 rad）；追踪弹/选项随 .sht 分期。
    for (int i = -1; i <= 1; ++i) {
        PlayerShot shot;
        shot.x = player_x;
        shot.y = player_y - 16.0f;
        shot.angle = -1.5707964f + 0.10f * float(i);
        shot.speed = kShotSpeed;
        shot.damage = kShotDamage;
        shot.alive = true;
        shots.push_back(shot);
    }
}

void StageBattle::step_shots() {
    for (auto& shot : shots) {
        if (!shot.alive) continue;
        shot.x += shot.speed * std::cos(shot.angle);
        shot.y += shot.speed * std::sin(shot.angle);
        if (shot.y < -32.0f || shot.x < -224.0f || shot.x > 224.0f) shot.alive = false;
    }
    size_t keep = 0;
    for (size_t i = 0; i < shots.size(); ++i)
        if (shots[i].alive) shots[keep++] = shots[i];
    shots.resize(keep);
}

void StageBattle::collide_shots() {
    for (auto& shot : shots) {
        if (!shot.alive) continue;
        for (auto& enemy : sim.enemies) {
            if (enemy->dead || enemy->life <= 0.0f) continue;
            const float ex = enemy->position[0], ey = enemy->position[1];
            const float hw = enemy->hurtbox[0], hh = enemy->hurtbox[1];
            if (shot.x < ex - hw || shot.x > ex + hw || shot.y < ey - hh || shot.y > ey + hh)
                continue;
            sim.damage(*enemy, shot.damage);
            shot.alive = false;
            if (enemy->dead) reward_kill(*enemy);
            break;
        }
    }
}

void StageBattle::collide_player() {
    if (player_dead || invulnerable > 0) return;
    // 敌弹。
    for (const auto& bullet : sim.bullets) {
        if (!bullet.alive) continue;
        const float dx = bullet.x - player_x, dy = bullet.y - player_y;
        const float radius = kBulletHitRadius + kPlayerHitRadius;
        if (dx * dx + dy * dy <= radius * radius) {
            kill_player();
            return;
        }
    }
    // 敌机本体。
    for (const auto& enemy : sim.enemies) {
        if (enemy->dead) continue;
        const float dx = enemy->position[0] - player_x, dy = enemy->position[1] - player_y;
        const float radius = kEnemyBodyRadius + kPlayerHitRadius;
        if (dx * dx + dy * dy <= radius * radius) {
            kill_player();
            return;
        }
    }
}

bool StageBattle::kill_player() {
    player_->deaths++;
    player_->power = player_->power > 100 ? player_->power - 100 : 100;  // 掉 1.00 灵力（近似）
    if (player_->extra_lives > 0) {
        player_->extra_lives--;
        if (!host_->life_hud(player_->extra_lives, player_->life_pieces)) return false;
        invulnerable = kInvulnFrames;
        sim.clear_bullets();  // 中弹清屏（原版灵界/复活演出未接入，保可玩）
        return true;
    }
    player_dead = true;
    return host_->request_game_over();
}

void StageBattle::reward_kill(StageEnemy& enemy) {
    score_->score += kKillScore + i32(enemy.life) * 10;
    player_->power = player_->power + kKillPower > player_->max_power
                         ? player_->max_power
                         : player_->power + kKillPower;
}

void StageBattle::on_bomb_key() {
    if (player_->bombs <= 0 || bomb_timer > 0 || player_dead) return;
    player_->bombs--;
    if (!host_->bomb_hud(player_->bombs, player_->bomb_pieces)) error_ = "bomb hud";
    bomb_timer = kBombFrames;
    invulnerable = kInvulnFrames < kBombFrames ? kBombFrames : kInvulnFrames;
    sim.clear_bullets();
}

bool StageBattle::step(const GameInput& input) {
    if (!error_.empty()) return false;
    if (invulnerable > 0) --invulnerable;
    if (!player_dead) move_player(input.held);
    if (!player_dead && (input.held & 0x01)) fire_shots();
    else if (fire_cooldown_ > 0) --fire_cooldown_;
    if (input.pressed & 0x02) on_bomb_key();
    step_shots();
    if (bomb_timer > 0) {
        --bomb_timer;
        for (auto& enemy : sim.enemies) {
            if (enemy->dead) continue;
            sim.damage(*enemy, kBombDamagePerFrame);
            if (enemy->dead) reward_kill(*enemy);
        }
    }
    // ECL 步进吃实时自机位置（etOn 瞄准/追弹用）。
    sim.player_x = player_x;
    sim.player_y = player_y;
    if (!sim.step_frame()) {
        error_ = sim.error();
        return false;
    }
    collide_shots();
    collide_player();
    return error_.empty();
}

}  // namespace th13
