// 标题帧回调（th15 TitleFrame 裁剪：原版优先级 6 更新；draw 回调承载标题期
// 场景级 @R 边条提交——oracle at-r-strips.md 实证的四条常量几何）。宿主拥有
// 异步文件、音频与 destination 队列。
#pragma once

#include "TitleState.hpp"
#include "TitleMainMenu.hpp"
#include "FrameScheduler.hpp"

namespace th13 {

struct TitleFrameInput {
    u32 held = 0, pressed = 0, repeated = 0, system_flags = 0;
};

struct TitleFramePlatform {
    virtual ~TitleFramePlatform() = default;
    virtual bool clear_title_overlay() = 0;
    virtual bool queue_title_music(i32 code, i32 value, const std::string&) = 0;
    virtual bool clear_current_wave() = 0;
    virtual bool title_exit(i32 destination) = 0;
    virtual bool fade_out_title_music() = 0;
};

struct TitleFrameServices : TitleFramePlatform {
    virtual bool update_title_menu(TitleScreen, const TitleFrameInput&) = 0;
};

// @R 边条 ×4 场景级常量提交（oracle at-r-strips.md；标题期与 boot 装载幕共用——
// 真机 f0-f599 每帧恒定，由四个场景 draw 任务的精灵栈提交，非 ANM 池 VM）。
// 仅绘制清单捕获态（draw_capture 非空）有副作用；text.anm 需已装载于 bank 0。
void emit_title_side_strips(AnmManager& animations);

class TitleFrame {
    TitleState& state;
    SessionState& progress;
    TitleFrameServices& host;
    FrameScheduler& scheduler;
    AnmManager& animations;
    std::array<FrameCallback, 2> callbacks;
    TitleFrameInput input;
    u32 white13_prev = 0;  // [wf360] w13 一帧记忆
    bool check(bool, const char*);
    bool menu();
    bool initialize();
    void submit_side_strips();  // @R 边条 ×4（场景级常量几何，见 .cpp）
    void adopt_logo_layers();   // [wf290] 97/98 翻转期层搬移（机制待定，蓝图 §8.8）

public:
    i32 music_age = 0;
    std::string error;
    TitleFrame(TitleState&, SessionState&, TitleFrameServices&, FrameScheduler&,
               AnmManager&);
    ~TitleFrame();
    void controls(const TitleFrameInput& value) noexcept { input = value; }
    bool update(const TitleFrameInput&);
    bool drawing() const noexcept { return callbacks[1].enabled; }
};

}  // namespace th13
