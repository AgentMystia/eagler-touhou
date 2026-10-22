// StageBattle 实现。ECL 步进与敌机/弹幕数据全部来自 StageSim；自机侧按
// event-contract.md §7.2 定点模型（0x442960 / pl02a.sht / 0x446870 同式）。
#include "StageBattle.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace th13 {

namespace {

u16 read_u16(const u8* p) {
    return u16(p[0]) | u16(p[1]) << 8;
}
u32 read_u32(const u8* p) {
    return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
}

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

// 0x443e40 的弹型域解析：+2 u16 块数、+0xc8 u32 指针表（+0xf4 基准的
// 相对偏移），每块为 0x38 步长的记录数组、首字节 i8 <0 终止。记录体
// （0x446000/0x445c40 直读）：+0 u8 间隔（兼类别）、+1 u8 相位、+2 u16
// 脚本、+4/+8 f32 出膛偏移、+0x14 f32 角度、+0x18 f32 伤害、+0x20 i8 锚位。
bool parse_sht_shots(const u8* sht, size_t size, StageBattle& battle) {
    if (size < 0xcc) return false;
    const u16 blocks = read_u16(sht + 2);
    if (blocks == 0 || blocks > StageBattle::kShtBlocks) return false;
    for (u32 block = 0; block < blocks; block++) {
        const u32 offset = read_u32(sht + 0xc8 + block * 4);
        size_t cursor = size_t(0xf4) + offset;
        std::vector<ShtShotRecord> records;
        for (u32 record = 0;; record++) {
            const size_t at = cursor + size_t(record) * 0x38;
            // 终止记录只占首字节（末块常紧贴文件尾，实测四张 .sht 的末块
            // 终止符落在 size-4 处）；完整记录体仍需 0x38 字节在界内。
            if (at >= size) return false;
            if (i8(sht[at]) < 0) break;
            if (at + 0x38 > size) return false;
            ShtShotRecord entry;
            entry.interval = i32(sht[at]);
            entry.phase = i32(sht[at + 1]);
            entry.script = i32(read_u16(sht + at + 2));
            entry.anchor = i32(static_cast<i8>(sht[at + 0x20]));
            std::memcpy(&entry.x, sht + at + 4, sizeof entry.x);
            std::memcpy(&entry.y, sht + at + 8, sizeof entry.y);
            std::memcpy(&entry.angle, sht + at + 0x14, sizeof entry.angle);
            std::memcpy(&entry.damage, sht + at + 0x18, sizeof entry.damage);
            records.push_back(entry);
        }
        battle.sht_shots[block] = std::move(records);
    }
    battle.sht_shots_loaded = true;
    return true;
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
        // 弹型域（0x443e40）：布局异常按装载失败处理（不静默降级）。
        if (!parse_sht_shots(sht.data(), sht.size(), *this)) {
            error_ = std::string("invalid shot table in ") + sht_name;
            return false;
        }
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

void StageBattle::fire_shots(i32 frame) {
    const auto spawn = [&](float x, float y, float damage, float angle, i32 script) {
        PlayerShot shot;
        shot.x = x;
        shot.y = y;
        shot.damage = damage;
        shot.angle = angle;
        shot.script = script;
        shot.alive = true;
        shots.push_back(shot);
    };
    if (sht_shots_loaded) {
        // 原版语义（replay-twin-divergence.md §3）：
        // 块 = 功率/步长（0x446000:13-16，power_step=100 出 sht+0x24；回放
        // 口径功率恒初值 100 → 块 1/6，拾取演化分期），聚焦 +5（sht+0x20=4）。
        i32 level = player_ ? player_->power / player_->power_step : 0;
        if (level < 0) level = 0;
        if (level > 4) level = 4;
        const u32 block = u32(level) + (focus ? 5u : 0u);
        // 临时探针（TH13_SHOT_PROBE=1）：P0 弹迹对拍（与 tools/shot_probe.py 同窗）。
        const char* probe = std::getenv("TH13_SHOT_PROBE");
        const bool shot_probe = probe && probe[0] == '1';
        for (const auto& record : sht_shots[block]) {
            // 0x446000:24：帧 % 间隔 == 相位才出膛（无冷却计数）。
            if (record.interval <= 0) continue;
            if (frame % record.interval != record.phase) continue;
            // 伤害 = 记录 +0x18（0x445c40:55）；anchor≠0 的卫星锚位记录
            // 孪生口径按自机位出膛（卫星拖尾模型分期）。
            spawn(player_x + record.x, player_y + record.y, record.damage,
                  record.angle, record.script);
            if (shot_probe && sim.frame <= 130)
                std::printf("PROBE fire ff=%d sim=%d pos=(%.2f,%.2f) dmg=%.1f\n",
                            frame, sim.frame, player_x + record.x, player_y + record.y,
                            double(record.damage));
        }
        return;
    }
    // 无 .sht 回退（交互直入未带 .sht）：三发直线近似，同相位制。
    if (frame % kFireInterval != 0) return;
    for (int i = -1; i <= 1; ++i)
        spawn(player_x + kShotOffsetX * float(i), player_y - kShotOffsetY, kShotDamage,
              -1.5707964f, -1);
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
    // 临时探针（TH13_SHOT_PROBE=1）：P0 命中检测对拍（与 stage1_oracle HITCHK 同窗）。
    const char* probe = std::getenv("TH13_SHOT_PROBE");
    const bool shot_probe = probe && probe[0] == '1';
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
            if (shot_probe && sim.frame <= 130)
                std::printf("HITCHK f=%d enemy=? shot=(%.2f,%.2f) enemy=(%.2f,%.2f) "
                            "hb=(%.2f,%.2f) dx=%.2f dy=%.2f\n",
                            sim.frame, shot.x, shot.y, ex, ey,
                            enemy->hurtbox[0], enemy->hurtbox[1], dx, dy);
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
    ++fire_frame_;  // 发射相位计数（1 基，交互/回放同拍）
    input_state = input.held;
    if (invulnerable > 0) --invulnerable;
    if (!player_dead) move_player(input.held);
    if (events_) sim.sink.player(player_x, player_y, input_state);
    // ECL 步进吃实时自机位置（etOn 瞄准/追弹用）。
    sim.player_x = player_x;
    sim.player_y = player_y;
    if (!player_dead && (input.held & 0x01)) fire_shots(fire_frame_);
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
