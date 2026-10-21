// StagePresentation 实现。锚点注释对应 docs/plans/anm-pipeline.md 与
// BulletAppearance.hpp（0x4120b0 / 0x4bbe30 表）。
#include "StagePresentation.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace th13 {

// 调试直绘过滤（默认 4 pass 全开）。
u32 StagePresentation::debug_pass_mask = 0xf;

namespace {
// 游戏坐标 → ANM 屏幕 coordinates（0x46f820 的 bit0 平移：+320/+16）。
constexpr float kGameToScreenX = 320.0f, kGameToScreenY = 16.0f;
// 弹体贴图基准角：贴图朝右时旋转 = 弹角（对屏检验后校准）。
constexpr float kBulletSpriteForward = 0.0f;
}  // namespace

StagePresentation::StagePresentation(AnmManager& manager, AnmRenderer& draw, Banks banks,
                                     i32 player_shot_sprite)
    : animations(manager), renderer(draw), banks(banks), player_shot_sprite_(player_shot_sprite) {}

void StagePresentation::release() {
    for (auto& enemy : enemies_)
        if (enemy.vm) animations.release_tree(*enemy.vm);
    enemies_.clear();
    bullets_.clear();
    shots_.clear();
    player_.reset();
}

bool StagePresentation::sync(StageBattle& battle, u32 held) {
    if (!error_.empty()) return false;
    auto* stage_resource = animations.resource(banks.stage);
    auto* bullet_resource = animations.resource(banks.bullet);
    auto* player_resource = animations.resource(banks.player);
    if (!stage_resource || !bullet_resource || !player_resource)
        return fail("Stage presentation banks unavailable");

    // ---- ECL 307/308 特效 VM（0x4243e0：模板拷贝→立即 tick→主池头入链；
    // layer=-1（脚本自定层，如神灵 script91 ins_304(2)）、anmArg=0、
    // 308 位置清零 / 307 敌机位。文件索引：0=bullet、1=effect、2..=stage 合并表）
    for (const auto& request : battle.sim.anm_effects) {
        const i32 bank = request.file == 0   ? banks.bullet
                         : request.file == 1 ? banks.effect
                                             : banks.stage;
        if (!animations.create(bank, request.script, -1, 2,
                               {request.x, request.y, request.z}, 0, false))
            return fail("Effect animation creation failed: " + animations.error);
    }
    battle.sim.anm_effects.clear();

    // ---- 敌机：ECL 记录的槽 0 脚本（全局序号），位置逐帧跟随 ----
    std::vector<u32> alive;
    for (auto& enemy : battle.sim.enemies) {
        if (enemy->dead) continue;
        alive.push_back(enemy->serial);
        EnemyVisual* slot = nullptr;
        for (auto& visual : enemies_)
            if (visual.serial == enemy->serial) {
                slot = &visual;
                break;
            }
        const i32 script = enemy->anm_script[0];
        if (script < 0) continue;  // 无本体脚本（纯发射器敌机）
        if (!slot) {
            enemies_.push_back(EnemyVisual{enemy->serial, -1, std::make_unique<AnmVm>()});
            slot = &enemies_.back();
        }
        if (!slot->vm->resource || slot->script != script) {
            slot->vm->environment = &animations.scene_environment();
            slot->vm->object_host = &animations;
            if (!slot->vm->bind(*stage_resource, u32(script)))
                return fail("Enemy animation binding failed: " + slot->vm->error);
            slot->script = script;
        }
        slot->vm->visual.translation = {enemy->position[0] + kGameToScreenX,
                                        enemy->position[1] + kGameToScreenY, enemy->position[2]};
        if (slot->vm->tick(random, 1) < 0) return fail("Enemy animation: " + slot->vm->error);
    }
    // 消失的敌机：销毁其 ANM 子树（子体可能仍挂 registry，release_tree 归还）。
    for (size_t i = 0; i < enemies_.size();) {
        bool keep = false;
        for (u32 serial : alive)
            if (enemies_[i].serial == serial) {
                keep = true;
                break;
            }
        if (keep) {
            ++i;
            continue;
        }
        if (enemies_[i].vm && !animations.release_tree(*enemies_[i].vm))
            return fail("Enemy animation release failed");
        enemies_.erase(enemies_.begin() + i);
    }

    // ---- 敌弹：bullet.anm 脚本 + 外观表精灵源（出生中断 2，0x40e840 尾）----
    if (bullets_.size() < battle.sim.bullets.size()) {
        const size_t wanted = battle.sim.bullets.size();
        while (bullets_.size() < wanted) bullets_.push_back(BulletSlot{});
    }
    size_t used = 0;
    for (const auto& bullet : battle.sim.bullets) {
        const auto* appearance = bullet_appearance(i32(bullet.sprite));
        BulletSlot& slot = bullets_[used++];
        slot.active = true;
        slot.source.type = i32(bullet.sprite);
        slot.source.color = i32(bullet.subtype);
        const i32 script = appearance ? appearance->script : -1;
        if (script < 0) continue;  // 未入表的弹型：无视觉（碰撞仍生效）
        if (!slot.vm) slot.vm = std::make_unique<AnmVm>();
        // 槽位复用（池按下标分配）：弹色变化 = 不同弹体，重绑并重放出生中断。
        if (!slot.vm->resource || slot.script != script || slot.color != i32(bullet.subtype)) {
            slot.vm->environment = &animations.scene_environment();
            slot.vm->object_host = &animations;
            if (!slot.vm->bind(*bullet_resource, u32(script)))
                return fail("Bullet animation binding failed: " + slot.vm->error);
            slot.vm->sprite_source = &slot.source;
            slot.vm->pending_interrupt = 2;  // 出生（0x40e840：vm+0x4c8 = 2）
            slot.script = script;
            slot.color = i32(bullet.subtype);
        }
        slot.vm->visual.translation = {bullet.x + kGameToScreenX, bullet.y + kGameToScreenY,
                                       bullet.z};
        slot.vm->variables.rotation.z = bullet.angle + kBulletSpriteForward;
        if (slot.vm->tick(random, 1) < 0) return fail("Bullet animation: " + slot.vm->error);
    }
    for (size_t i = used; i < bullets_.size(); ++i) bullets_[i].active = false;

    // ---- 自机弹：.sht 弹型脚本（plXX.anm，旋转模式 1 随弹向）；无 .sht
    // 会话回退精灵直贴（player_shot_sprite_） ----
    if (shots_.size() < battle.shots.size())
        while (shots_.size() < battle.shots.size()) shots_.push_back(std::make_unique<AnmVm>());
    for (size_t i = 0; i < shots_.size(); ++i) {
        AnmVm& vm = *shots_[i];
        if (i < battle.shots.size() && battle.shots[i].alive) {
            const auto& shot = battle.shots[i];
            const i32 script = shot.script;
            if (script >= 0 && vm.source_script != script) {
                vm.environment = &animations.scene_environment();
                vm.object_host = &animations;  // 弹型脚本可 spawn 附属弹子 VM
                if (!vm.bind(*player_resource, u32(script)))
                    return fail("Player shot animation binding failed: " + vm.error);
            } else if (script < 0 && (!vm.resource || vm.source_script >= 0)) {
                vm.environment = &animations.scene_environment();
                vm.initialize_unbound();
                vm.resource = player_resource;
                if (!vm.select_sprite(player_shot_sprite_))
                    return fail("Player shot sprite unavailable");
                vm.visible = true;
                vm.visual.flags |= 1;
                vm.visual.render_flags |= 0x200;
            }
            vm.visual.translation = {shot.x + kGameToScreenX, shot.y + kGameToScreenY, 0.0f};
            vm.variables.rotation.z = shot.angle;  // 脚本旋转模式 1：贴图随弹向
            if (script >= 0 && vm.resource && vm.tick(random, 1) < 0)
                return fail("Player shot animation: " + vm.error);
        } else {
            vm.visible = false;
        }
    }

    // ---- 自机：plXX.anm 脚本 0/1/2（直立/左倾/右倾）----
    if (!battle.player_dead) {
        i32 tilt = 0;
        if (held & 0x40) tilt = 1;
        if (held & 0x80) tilt = 2;
        if (!player_) player_ = std::make_unique<AnmVm>();
        if (!player_->resource || tilt != player_tilt_) {
            player_->environment = &animations.scene_environment();
            player_->object_host = &animations;
            if (!player_->bind(*player_resource, u32(tilt)))
                return fail("Player animation binding failed: " + player_->error);
            player_tilt_ = tilt;
        }
        player_->visual.translation = {battle.player_x + kGameToScreenX,
                                       battle.player_y + kGameToScreenY, 0.0f};
        if (player_->tick(random, 1) < 0) return fail("Player animation: " + player_->error);
    } else if (player_) {
        player_.reset();
    }
    return true;
}

std::string StagePresentation::debug_report() const {
    std::string report;
    char line[256];
    std::snprintf(line, sizeof line, "enemies=%zu bullets=%zu shots=%zu player=%d\n",
                  enemies_.size(),
                  bullets_.empty() ? 0 : std::count_if(bullets_.begin(), bullets_.end(),
                                                       [](const BulletSlot& slot) {
                                                           return slot.active && slot.vm;
                                                       }),
                  shots_.size(), player_ ? 1 : 0);
    report += line;
    auto describe = [&](const char* kind, const AnmVm& vm, i32 type, i32 color) {
        const auto* source = vm.sprite_resource ? vm.sprite_resource : vm.resource;
        std::snprintf(line, sizeof line,
                      "  %s script=%d sprite=%d size=%.0fx%.0f scale=%.2fx%.2f "
                      "pos=(%.0f,%.0f) rot=%.2f mode=%u blend=%u type=%d color=%d\n",
                      kind, vm.source_script, vm.visual.sprite,
                      double(vm.visual.sprite_size.x), double(vm.visual.sprite_size.y),
                      double(vm.visual.scale.x), double(vm.visual.scale.y),
                      double(vm.visual.translation.x), double(vm.visual.translation.y),
                      double(vm.variables.rotation.z), unsigned(vm.visual.draw_mode()),
                      unsigned(vm.visual.blend_mode()), type, color);
        report += line;
        (void)source;
    };
    for (const auto& enemy : enemies_)
        if (enemy.vm) describe("enemy", *enemy.vm, enemy.script, -1);
    u32 shown = 0;
    for (const auto& slot : bullets_) {
        if (!slot.active || !slot.vm || shown >= 40) continue;
        describe("bullet", *slot.vm, slot.source.type, slot.source.color);
        ++shown;
    }
    if (!shots_.empty() && shots_[0])
        describe("shot", *shots_[0], player_shot_sprite_, -1);
    if (player_) describe("player", *player_, player_script_, player_tilt_);
    return report;
}

bool StagePresentation::draw(u32 pass) {
    if (!error_.empty()) return false;
    if (!(debug_pass_mask >> pass & 1u)) return true;  // 调试过滤
    const auto one = [&](AnmVm& vm) {
        if (!vm.visible) return true;
        return renderer.draw(vm) != -2 || fail(renderer.error.empty() ? "Stage draw failed"
                                                                      : renderer.error);
    };
    switch (pass) {
    case 0:
        for (auto& shot : shots_)
            if (shot) one(*shot);
        return error_.empty();
    case 1: return player_ ? one(*player_) : true;
    case 2:
        for (auto& enemy : enemies_)
            if (enemy.vm) one(*enemy.vm);
        return error_.empty();
    case 3:
        for (auto& slot : bullets_)
            if (slot.active && slot.vm) one(*slot.vm);
        return error_.empty();
    default: return fail("Stage draw pass outside schedule");
    }
}

}  // namespace th13
