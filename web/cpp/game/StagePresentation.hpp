// Stage1 战斗表现层：把 StageSim/StageBattle 的状态同步为 ANM VM 并直绘。
// 敌机 = ECL 302/303/306 记录的脚本（stage ANM 合并表全局序号）；
// 敌弹 = bullet.anm 脚本 + 0x4120b0 精灵解析（BulletAppearance 表）；
// 自机/自机弹 = plXX.anm（脚本 0/1/2 姿态；弹为精灵直贴）。
// VM 不入 registry（除脚本自派的子体经 object_host 入链），绘制直调
// AnmRenderer（th15 PlayerDraw/GameDraw 同式），由 RunScene 注册分层回调。
#pragma once

#include "AnmManager.hpp"
#include "AnmRenderer.hpp"
#include "BulletAppearance.hpp"
#include "GameRandom.hpp"
#include "StageBattle.hpp"

#include <memory>
#include <vector>

namespace th13 {

class StagePresentation {
public:
    struct Banks {
        i32 stage = 0, bullet = 0, player = 0;
        i32 effect = 8;  // effect.anm（mgr 槽 8；ECL 文件索引 1 的特效创建源）
    };
    // player_shot_sprite：无 .sht 会话的自机弹回退精灵（plXX.anm 内）。
    StagePresentation(AnmManager& animations, AnmRenderer& renderer, Banks banks,
                      i32 player_shot_sprite);
    // 帧同步（Update pass 内调用）：敌机脚本接线、弹体/自机弹/自机姿态。
    bool sync(StageBattle& battle, u32 held);
    // true = 敌机本体 VM 由 ECL 303/306 创建入池（0x41fe80/case3 同构），
    // sync 的敌机接线/清理段跳过（draws 对拍模式；web 旧路径 false）。
    bool enemies_from_ecl = false;
    // 直绘 pass：0=自机弹 1=自机 2=敌机 3=敌弹（调用方保证 Fullscreen 相机）。
    bool draw(u32 pass);
    void release();  // 场景卸载：销毁敌机子树（外部 VM 归还 registry 外部表）
    const std::string& error() const { return error_; }
    // 画面诊断：敌机/敌弹/自机弹/自机 VM 清单（脚本/精灵/尺寸/缩放/位置）。
    std::string debug_report() const;
    // 调试直绘过滤：bit0..3 = 自机弹/自机/敌机/敌弹；默认全开。
    static u32 debug_pass_mask;

private:
    struct EnemyVisual {
        u32 serial = 0;
        i32 script = -1;
        std::unique_ptr<AnmVm> vm;
    };
    struct BulletSlot {
        std::unique_ptr<AnmVm> vm;
        i32 script = -1;
        i32 color = -1;  // 槽位当前弹色（变色即重绑 → 出生中断重放）
        bool active = false;
        // 0x4120b0 精灵解析源：参数 → 表[类型].色[颜色]
        struct Source final : AnmSpriteSource {
            i32 type = 0, color = 0;
            i32 sprite_index(i32 parameter) const noexcept override {
                return bullet_animation_parameter(type, color, parameter);
            }
        } source;
    };
    AnmManager& animations;
    AnmRenderer& renderer;
    Banks banks;
    // 与管理器（=ECL sim）同一条 0x472c90 游戏侧 RNG：原版敌机/弹体/自机弹
    // VM 全在全局态 0x4dc31c 上抽签，独立种子会移动共享流（对拍实证）。
    GameRandom& random;
    std::vector<EnemyVisual> enemies_;
    std::vector<BulletSlot> bullets_;
    std::vector<std::unique_ptr<AnmVm>> shots_;
    std::unique_ptr<AnmVm> player_;
    i32 player_script_ = -1, player_tilt_ = 0;
    i32 player_shot_sprite_;
    std::string error_;
    bool fail(const std::string& why) {
        if (error_.empty()) error_ = why;
        return false;
    }
};

}  // namespace th13
