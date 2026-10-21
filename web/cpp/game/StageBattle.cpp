// StageBattle 实现。ECL 步进与敌机/弹幕数据全部来自 StageSim；自机侧按
// event-contract.md §7.2 定点模型（0x442960 / pl02a.sht / 0x446870 同式）。
#include "StageBattle.hpp"

#include <cmath>
#include <cstring>

namespace th13 {

namespace {

// 0x4abc30 方向表（单位向量，斜向不归一）：0 无 1↑ 2↓ 3← 4→ 5↖ 6↗ 7↙ 8↘。
const i32 kDirX[9] = {0, 0, 0, -1, 1, -1, 1, -1, 1};
const i32 kDirY[9] = {0, -1, 1, 0, 0, -1, -1, 1, 1};

// 0x442960:55-78 的方向判定序（复合方向优先，单一方向按位）。
int dir_index(u32 held) {
    if ((held & 0x50) == 0x50) return 5;
    if ((held & 0x60) == 0x60) return 7;
    if ((held & 0x90) == 0x90) return 6;
    if ((held & 0xa0) == 0xa0) return 8;
    if ((held & 0x20) == 0) {
        if ((held & 0x10) == 0) {
            if ((held & 0x40) == 0) return (held & 0x80) ? 4 : 0;
            return 3;
        }
        return 1;
    }
    return 2;
}

// 0x441eae：speed128 = int(sht 浮点 × 128.0)（截断）。
i32 speed128_from_float(float value) {
    return i32(value * 128.0f);
}

}  // namespace

bool StageBattle::load(EclResourceProvider& provider, const char* stage_file, i32 difficulty_rank,
                       StageBattleServices& host, SessionState& progress, HudPlayerState& player,
                       HudScoreState& score, const char* event_path, const char* sht_name) {
    host_ = &host;
    progress_ = &progress;
    player_ = &player;
    score_ = &score;
    player_x = 0.0f;
    player_y = 400.0f;
    pos_fx = 0;
    pos_fy = 51200;
    invulnerable = kInvulnFrames;
    if (sht_name) {
        // §7.2：移速出自 .sht（+0x10..+0x1c 四浮点），文件缺失即失败（不静默）。
        std::vector<u8> sht;
        if (!provider.read(sht_name, sht) || sht.size() < 0x20) {
            error_ = std::string("cannot read ") + sht_name;
            return false;
        }
        float fields[4];
        for (int i = 0; i < 4; ++i)
            std::memcpy(&fields[i], sht.data() + 0x10 + i * 4, sizeof fields[i]);
        speed_orth = speed128_from_float(fields[0]);
        speed_orth_focus = speed128_from_float(fields[1]);
        speed_diag = speed128_from_float(fields[2]);
        speed_diag_focus = speed128_from_float(fields[3]);
    }
    if (!sim.begin(provider, stage_file, difficulty_rank, event_path)) {
        error_ = sim.error();
        return false;
    }
    events_ = event_path != nullptr;
    if (!sim.start()) {
        error_ = sim.error();
        return false;
    }
    if (events_) sim.sink.set_frame(0);
    return true;
}

void StageBattle::move_player(u32 held) {
    focus = (held & 0x08) != 0;
    const int dir = dir_index(held);
    const i32 speed = dir == 0 ? 0
                                  : focus ? (dir < 5 ? speed_orth_focus : speed_diag_focus)
                                          : (dir < 5 ? speed_orth : speed_diag);
    // 0x442960:223-236：定点累加 + 钳制（±0x5c00 / [0x1000,0xd800]）。
    pos_fx += speed * kDirX[dir];
    pos_fy += speed * kDirY[dir];
    if (pos_fx < -0x5c00) pos_fx = -0x5c00;
    else if (pos_fx > 0x5c00) pos_fx = 0x5c00;
    if (pos_fy < 0x1000) pos_fy = 0x1000;
    else if (pos_fy > 0xd800) pos_fy = 0xd800;
    player_x = float(pos_fx) * 0.0078125f;
    player_y = float(pos_fy) * 0.0078125f;
}

void StageBattle::fire_shots() {
    if (fire_cooldown_ > 0) {
        --fire_cooldown_;
        return;
    }
    fire_cooldown_ = kFireInterval;
    for (int i = -1; i <= 1; ++i) {
        PlayerShot shot;
        shot.x = player_x + kShotOffsetX * float(i);
        shot.y = player_y - kShotOffsetY;
        shot.damage = kShotDamage;
        shot.alive = true;
        shots.push_back(shot);
    }
}

void StageBattle::step_shots() {
    for (auto& shot : shots) {
        if (!shot.alive) continue;
        shot.y -= kShotSpeed;  // 垂直直进（整值步进，无浮点误差）
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
            // 0x419a90:123 门（flags & 0x21 == 0 才可伤）+ 0x446870 AABB。
            if (enemy->flags & 0x21u) continue;
            const float ex = enemy->position[0], ey = enemy->position[1];
            const float hw = enemy->hurtbox[0] * 0.5f + kShotHalf;
            const float hh = enemy->hurtbox[1] * 0.5f + kShotHalf;
            const float dx = shot.x - ex, dy = shot.y - ey;
            if (dx < -hw || dx > hw || dy < -hh || dy > hh) continue;
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
    // 事件模式帧推进：0 帧是 start() 装载趟，此后每 step 一帧（§7.3）。
    if (events_) {
        ++sim.frame;
        sim.sink.set_frame(sim.frame);
    }
    input_state = input.held;
    if (invulnerable > 0) --invulnerable;
    if (!player_dead) move_player(input.held);
    if (events_) sim.sink.player(player_x, player_y, input_state);
    // ECL 步进吃实时自机位置（etOn 瞄准/追弹用）。
    sim.player_x = player_x;
    sim.player_y = player_y;
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
    if (!sim.step_frame()) {
        error_ = sim.error();
        return false;
    }
    collide_shots();
    collide_player();
    return error_.empty();
}

}  // namespace th13
