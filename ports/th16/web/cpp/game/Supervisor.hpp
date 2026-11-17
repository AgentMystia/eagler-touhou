// th16 Supervisor——原版静态上下文 DAT_004c10d0 上的场景状态机。
//
// 锚点：
//   0x43ba40  注册主更新 0x43b3d0（优先级 1）与若干绘制链
//   0x43b3d0  每帧：音频泵 / 输入 0x401d50 / 装载步进 / 场景切换 0x43ce10
//   0x43ce10  +0x6f0 当前 / +0x6f4 请求；全局请求 DAT_004c17c4
//   0x43b1f0  创建启动幕对象；0x43adc0 装载；完成后启用更新链 → 0x43b290 请求 Title(4)
//   0x44aee0  创建标题；0x44abc0 线程：装载 title*.anm，等启动幕 +0x638≥0xb4 或跳过输入，
//            再卸 sig.anm、启用标题更新链
//
// 本文件只还原状态与时序；具体装载/生成由 Application 回调完成。
#pragma once
#include "Input.hpp"
#include "Types.hpp"

#include <cstdint>

namespace th16 {

enum class SceneId : i32 {
    Invalid = -1,
    Boot = 1,     // 0x43ce10 case 0→1 后的当前态
    Title = 4,    // DAT_004c17c4 = 4（0x43b290）
};

class Supervisor {
public:
    static constexpr u32 kBootHoldFrames = 0xb4;  // 180：标题线程等待启动幕帧计数

    SceneId current = SceneId::Invalid;
    SceneId requested = SceneId::Invalid;
    float game_speed = 1.0f;   // DAT_004a5788（0x43b520 置 1.0）
    InputState input;
    u32 boot_frames = 0;       // 启动幕对象 +0x638（0x43b300 每绘帧 +1）
    bool title_assets_ready = false;  // title.anm / title_v.anm 已进槽
    bool title_menu_live = false;     // 已卸 sig、生成菜单脚本

    void request(SceneId id) { requested = id; }

    // 0x43b290：启动幕装载完成后请求标题
    void boot_init_finished() { request(SceneId::Title); }

    // 标题线程等待条件（0x44abea / 0x44ac00）
    bool boot_hold_elapsed() const {
        return boot_frames >= kBootHoldFrames || input.boot_skip();
    }

    // 若 requested != current 则切换并返回 true（调用方执行 enter/leave）
    bool consume_switch(SceneId& out_leave, SceneId& out_enter) {
        if (requested == SceneId::Invalid || requested == current) return false;
        out_leave = current;
        out_enter = requested;
        current = requested;
        return true;
    }
};

}  // namespace th16
