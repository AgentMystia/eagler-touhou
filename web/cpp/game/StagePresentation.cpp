// StagePresentation 实现。锚点注释对应 docs/plans/anm-pipeline.md 与
// BulletAppearance.hpp（0x4120b0 / 0x4bbe30 表）。
#include "StagePresentation.hpp"

#include <algorithm>
#include <cmath>

namespace th13 {

// 调试直绘过滤（4 pass 全开）。
u32 StagePresentation::debug_pass_mask = 0xf;

namespace {
// 游戏坐标 → ANM 屏幕坐标：+320/+16 屏幕原点已由 0x40000 位在底层
// adjust_position 统一累加（th13 .data 双精度常量 0x4ae9f0=320.0 /
// 0x4ae8e0=16.0——0x466a80/0x468670/0x46f820 族一致；th15 遗产的 224
// 是 f61 敌机行走位差 96px 的根因，2026-10-23 实测修正），此处外部平移
// 增量置 0，杜绝双重叠加。
constexpr float kGameToScreenX = 0.0f, kGameToScreenY = 0.0f;
// 弹体贴图基准角：贴图朝右时旋转 = 弹角（对屏检验后校准）。
constexpr float kBulletSpriteForward = 0.0f;
}  // namespace

StagePresentation::StagePresentation(AnmManager& manager, AnmRenderer& draw, Banks banks,
                                     i32 player_shot_sprite)
    : animations(manager), renderer(draw), banks(banks),
      random(manager.random_source()), player_shot_sprite_(player_shot_sprite) {}

void StagePresentation::release() {
    for (auto& enemy : enemies_)
        if (enemy.vm) animations.release_tree(*enemy.vm);
    enemies_.clear();
    bullets_.clear();
    for (auto& shot : shots_)
        if (shot && shot->resource) animations.release_tree(*shot);
    shots_.clear();
    if (player_) {
        animations.release_tree(*player_);
        player_.reset();
    }
}

// 30x/镜像创建点色通道补步：原版这些 VM 在敌机更新趟内创建（0x41fe80
// case3 与 0x419450 镜像块，均由 0x41a8a0 趟直调），创建趟本身多推进一次
// 色/α 插值——oracle 实测 f0 VM+0x15c α=6/timer(+0x160)=2 而 port 4/1
//（f9 第二个环境精灵同症——非装载趟特例，是每次创建的普遍行为）。
// 仅色/α 两通道：整池 update 会推 world 矩阵（实测破坏 f0 对齐），脚本
// 与其余通道不动（见 AnmInterpolators::advance_color）。
void StagePresentation::create_color_step(AnmVm* vm) {
    if (vm) vm->interpolators.advance_color(vm->visual, animations.rate);
}

bool StagePresentation::sync(StageBattle& battle, u32 held) {
    if (!error_.empty()) return false;
    auto* stage_resource = animations.resource(banks.stage);
    auto* bullet_resource = animations.resource(banks.bullet);
    auto* player_resource = animations.resource(banks.player);
    if (!stage_resource || !bullet_resource || !player_resource)
        return fail("Stage presentation banks unavailable");

    // ---- ECL 30x ANM 创建（0x4243e0：模板拷贝→立即 tick→主池头入链。
    // kind=0 特效路径 layer=-1（脚本自定层）、anmArg=0、位置清零（307 的
    // 敌机位仅特效路径用）；kind=1 槽位路径见下方 [+0x278]+7 注。
    // 文件索引：0=bullet、1=effect、2..=stage 合并表（脚本号文件内局部——
    // 文件 3+ 的合并偏移为已知局限，窗口内敌机均用文件 2）----
    for (const auto& request : battle.sim.anm_effects) {
        const i32 bank = request.file == 0   ? banks.bullet
                         : request.file == 1 ? banks.effect
                                             : banks.stage;
        if (request.kind == 0) {
            const u32 effect_handle =
                animations.create(bank, request.script, -1, 2,
                                  {request.x, request.y, request.z}, 0, false);
            if (!effect_handle)
                return fail("Effect animation creation failed: " + animations.error);
            create_color_step(animations.registry.find(effect_handle));
            continue;
        }
        // kind=1：303/306 槽位 VM（0x41fe80 / 0x41b260 case3）——重挂先回收
        // 旧槽（0x470110 语义：retire+子树 0x20 位），script<0 仅清槽。
        StageEnemy* enemy = nullptr;
        for (auto& candidate : battle.sim.enemies)
            if (candidate->serial == request.serial) {
                enemy = candidate.get();
                break;
            }
        if (!enemy) continue;
        u32& handle = enemy->anm_slot_handle[request.slot];
        if (handle) {
            if (!animations.retire(handle))
                return fail("Slot animation retire failed: " + animations.error);
            handle = 0;
        }
        if (request.script < 0) continue;
        // 槽位 VM 创建层 = 敌机内嵌 VM+0x278 + 7（0x41fe80:0x41ff24 `mov
        // 0x278(%esi),%eax; add $0x7,%eax`——脚本模板不自带层，此前传 -1
        // （脚本自定层）使 aura 挂错层）。该字段构造默认 1（StageEnemy
        // mirror_script_base，ECL 552 可覆盖）→ 层 8（与 oracle 敌机
        // aura up 层 4/8 对拍）。
        handle = animations.create(bank, request.script,
                                   enemy->mirror_script_base + 7, 2, {}, 0, false);
        if (!handle)
            return fail("Slot animation creation failed: " + animations.error);
        create_color_step(animations.registry.find(handle));
        if (request.slot == 0) {
            if (AnmVm* vm = animations.registry.find(handle)) {
                const float w = float(vm->visual.sprite_size.x *
                                      vm->visual.scale.x);
                const float h = float(vm->visual.sprite_size.y *
                                      vm->visual.scale.y);
                // 0x41fe80: +0x3f30=(vm+0x84)×(vm+0x64)、+0x3f34=(vm+0x80)×(vm+0x60)
                enemy->anm_size_a[0] = h;
                enemy->anm_size_a[1] = w;
                if (request.opcode == 306) {
                    enemy->anm_size_b[0] = h;
                    enemy->anm_size_b[1] = w;
                }
            }
        }
    }
    battle.sim.anm_effects.clear();
    // 槽位 VM 位置跟随（0x41a8a0:84-93：敌机管理器每帧把敌机 ECL 位置
    // （enemy+0x44/48/4c）写入各槽 VM 的偏移 +0x574/578/57c——0x466a80 的
    // 位置和项；+320/+16 由 0x40000 位在 adjust 阶段补，此处只写 ECL 原值）。
    if (enemies_from_ecl) {
        for (auto& enemy : battle.sim.enemies) {
            // 镜像翻转重挂（0x4196d3-0x419827 对齐反汇编全解）：
            // 门 = flags&0x200000 且 x 速度符号（+0x78 语义，位置差分近似）
            // 变化；行走 VM 脚本号 = 槽 0 脚本号 + 朝向增量查表（旧 0：
            // 新 -1→1 / 新 0,+1→2；旧 +1：恒 4；旧 -1：恒 3）；layer 恒 8；
            // 位置 = 旧槽 VM 位置（继承，非零）；anmArg=0；尾入池句柄存回槽 0。
            if (enemy->flags & 0x200000u) {
                const float dx = enemy->position[0] - enemy->mirror_prev_x;
                const i32 dir = dx > 0.03f ? 1 : (dx < -0.03f ? -1 : 0);
                if (dir != enemy->mirror_dir) {
                    i32 step;
                    if (enemy->mirror_dir == 0)
                        step = dir == -1 ? 1 : 2;
                    else if (enemy->mirror_dir == 1)
                        step = 4;
                    else
                        step = 3;
                    u32& handle = enemy->anm_slot_handle[0];
                    // 位置继承 = 翻转帧的敌机 ECL 位（原版读旧槽 VM+0x574，
                    // 其值即当帧跟随位——本 sync 尚未写跟随，故直接取敌机位）
                    const Vec3 old_pos{enemy->position[0], enemy->position[1],
                                       enemy->position[2]};
                    if (handle) {
                        if (!animations.retire(handle))
                            return fail("Mirror retire failed: " + animations.error);
                        handle = 0;
                    }
                    const i32 bank = enemy->anm_file == 0   ? banks.bullet
                                     : enemy->anm_file == 1 ? banks.effect
                                                           : banks.stage;
                    // 槽 0 脚本号 = 306 记录（enemy->anm_script[0]）；
                    // layer = 敌机内嵌 VM+0x278 + 7（0x419450:0x419764 实证，
                    // mirror_script_base 构造默认 1 → 层 8，ECL 552 可覆盖）；
                    // 插入方向 = 主链尾插（0x419764 块直调 0x46f3f0——f92
                    // Unicorn 实测 F3F0-tail，重挂 VM 落链尾、层桶序排旧 VM
                    // 之后。此前沿用 create 默认 ordering=2（0x46f4a0 头插
                    // 语义；303/306 建池 0x4243e0 才是头插）→ f92 op[12] 的
                    // 层内序 Blue 先于 Red，与 oracle 相反（draws 首分歧）。
                    handle = animations.create(bank,
                                               enemy->anm_script[0] + step,
                                               enemy->mirror_script_base + 7,
                                               0, old_pos, 0, false);
                    if (!handle)
                        return fail("Mirror animation creation failed: " +
                                    animations.error);
                    create_color_step(animations.registry.find(handle));
                    enemy->mirror_dir = dir;
                }
                enemy->mirror_prev_x = enemy->position[0];
            }
            for (u32 handle : enemy->anm_slot_handle)
                if (AnmVm* vm = animations.registry.find(handle)) {

                    vm->visual.translation = {enemy->position[0],
                                              enemy->position[1],
                                              enemy->position[2]};
                }
        }
    }

    // ---- 敌机：ECL 记录的槽 0 脚本（全局序号），位置逐帧跟随 ----
    //（enemies_from_ecl 时整段跳过：敌机本体 VM 由 ECL 303/306 创建入池，
    // 与 oracle 极小环境同构——对拍窗语义）
    std::vector<u32> alive;
    if (!enemies_from_ecl) {
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
    }  // !enemies_from_ecl

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
                if (vm.resource) animations.release_tree(vm);
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
                vm.visual.set_layer(11);
            }
            vm.visual.translation = {shot.x + kGameToScreenX, shot.y + kGameToScreenY, 0.0f};
            vm.variables.rotation.z = shot.angle;  // 脚本旋转模式 1：贴图随弹向
            if (script >= 0 && vm.resource && vm.tick(random, 1) < 0)
                return fail("Player shot animation: " + vm.error);
        } else {
            if (vm.resource) {
                animations.release_tree(vm);
                vm = AnmVm{};
            }
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
            player_->visual.set_layer(12);
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
    // [2026-10-24 拔除 -96 平移 hack] 旧注释称「原版 stage 上下文视口原点
    // X=+96、表现层渲染位置 = ECL+224（图像实证 f901 自机 ~178）」并据此对
    // 四 pass 直绘统一 renderer.offset -= 96——该「实证」的参照系是**裁剪
    // 错误的 golden**（速通视频缺游戏左侧 ~97px，2026-10-24 勘误，见
    // scripts/video_golden_extract.py 头注）。oracle 数据反证：st01-draws
    // -replay.jsonl f124 敌机 up 批 verts 屏幕坐标 x∈[411.5,443.5]（layer
    // 56，ECL≈+107 → ECL+320 直出），[32,416] 视口假设下该敌机出界。原版
    // 表现层 = ECL+320 直绘于 playfield [128,512]×[16,464]，无 -96 平移。
    // 同步修正：ScreenViews 游戏相机视口 32→128。
    const auto one = [&](AnmVm& vm) {
        if (!vm.visible) return true;
        return renderer.draw(vm) != -2 || fail(renderer.error.empty() ? "Stage draw failed"
                                                                      : renderer.error);
    };
    switch (pass) {
    case 0:
        for (auto& shot : shots_)
            if (shot) one(*shot);
        break;
    case 1:
        if (player_) one(*player_);
        break;
    case 2:
        for (auto& enemy : enemies_)
            if (enemy.vm) one(*enemy.vm);
        break;
    case 3:
        for (auto& slot : bullets_)
            if (slot.active && slot.vm) one(*slot.vm);
        break;
    default:
        return fail("Stage draw pass outside schedule");
    }
    return error_.empty();
}

}  // namespace th13
