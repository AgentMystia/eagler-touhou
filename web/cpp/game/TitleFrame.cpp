#include "TitleFrame.hpp"
#include "DrawCapture.hpp"

namespace th13 {

TitleFrame::TitleFrame(TitleState& s, SessionState& p, TitleFrameServices& h, FrameScheduler& f,
                       AnmManager& a)
    : state(s), progress(p), host(h), scheduler(f), animations(a) {
    callbacks[0].owner = callbacks[1].owner = this;
    callbacks[0].enabled = true;
    callbacks[1].enabled = true;
    callbacks[0].run = [](void* p) {
        auto& frame = *static_cast<TitleFrame*>(p);
        return frame.update(frame.input) ? i32(FrameAction::Continue) : i32(FrameAction::Error);
    };
    callbacks[1].run = [](void* p) {
        auto& frame = *static_cast<TitleFrame*>(p);
        frame.submit_side_strips();
        return i32(FrameAction::Continue);
    };
    // draw 回调优先级 2：先于全部 ANM 层走（AnmDrawSchedule entries 最低 5）——
    // 与真机「场景 draw 任务先提交 @R 边条、后走 mgr 逐层」同序（at-r-strips.md
    // 提交栈：4 条 up 均出场景任务，落在帧 op 序首）。
    if (scheduler.add(callbacks[0], FramePass::Update, 6) < 0 ||
        scheduler.add(callbacks[1], FramePass::Draw, 2) < 0)
        check(false, "Title callback registration failed");
}

TitleFrame::~TitleFrame() {
    for (auto& callback : callbacks) scheduler.remove(callback);
}

bool TitleFrame::check(bool ok, const char* why) {
    if (!ok && error.empty()) error = why;
    return ok;
}

bool TitleFrame::menu() { return check(host.update_title_menu(state.screen, input), "Title menu update failed"); }

bool TitleFrame::initialize() {
    if (!check(host.clear_title_overlay(), "Title text overlay release failed")) return false;
    const i32 reason = state.return_reason;
    auto main = [&]() {
        state.change_screen(TitleScreen::Main);
        state.return_reason = 1;
        callbacks[1].enabled = true;
        return menu();
    };
    if (reason == 0) {
        // 冷启动（真机 boot）：无提示幕，走完整时序——开场 logo 幕 180 帧后
        // 标题展开、菜单 intro 180 帧插值（oracle g181/g361/g372 锚点）。
        state.flags &= ~2u;
        return main();
    }
    // 自对局/回放返回（return_reason 值域待 supervisor 反汇编锚定，蓝图 §五-9）：
    // 菜单直接展开（背景复位根 script95、无 180 帧等待——推定，待 oracle）。
    state.flags |= 2u;
    return main();
}

bool TitleFrame::update(const TitleFrameInput& value) {
    if (!error.empty()) return false;
    input = value;
    switch (i32(state.screen)) {
    case 0:  // Initialize
        if (!initialize()) return false;
        break;
    case 1:  // Main
        callbacks[1].enabled = true;
        if (!menu()) return false;
        break;
    case 2:  // Quit / StartGame
        if (state.screen == TitleScreen::Quit) {
            if (!check(host.title_exit(3), "Title exit destination failed")) return false;
            if (!check(host.fade_out_title_music(), "Title exit music fade failed")) return false;
        }
        break;
    case 6:  // Difficulty
    case 7:  // Character
        if (!menu()) return false;
        break;
    default:
        break;
    }
    state.age.tick(&progress.rate);
    return true;
}

// @R 边条（oracle at-r-strips.md，窗A f0-f599 每帧恒定 4 个 up op）：
// 提交者是真机四个场景 draw 任务的精灵栈（非 ANM 池 VM），纹理 = text.anm
// （槽 0）entry2/3 的 "@R" 全屏离屏渲染目标。几何/混合/层残值按报告③表逐字段
// 落码；RT 像素内容两侧均不比对（契约不比像素）。仅绘制清单捕获态（web 构建
// draw_capture 恒空 → 无视觉副作用；web 真实渲染不画不透明白条以免糊屏）。
// 注：boot 期（oracle f1-179）四条同在——该段由 tests/harness 的装载幕镜像
// 渲染（非本类所有权），已登记为 harness 镜像同步项。
void TitleFrame::submit_side_strips() {
    if (!draw_capture) return;
    auto* text = animations.resource(0);  // text.anm 常驻槽 0（Application kBanks）
    if (!text) return;
    struct Strip {
        u32 entry, layer;
        float x0, x1;
    };
    // 条 1-3 同几何（127.5..511.5），条 4 整体左移 96px；UV 窗 (0.2,1/30)-(0.8,29/30)。
    const Strip strips[4] = {{2, 29, 127.5f, 511.5f}, {3, 30, 127.5f, 511.5f},
                             {2, 30, 127.5f, 511.5f}, {3, 20, 31.5f, 415.5f}};
    struct Vertex { float x, y, z, rhw; u32 diffuse; float u, v; };
    draw_capture->set_layout(touhou::graphics::VertexLayout::ScreenColorUv);
    for (const auto& strip : strips) {
        // 管线态影子的 blend 缺省 (One,Zero,Add) → 契约 [2,1,1]，与 oracle 恒等。
        draw_capture->bind_texture(draw_capture->texture(*text, strip.entry));
        draw_capture->set_stale_layer(i32(strip.layer));
        const Vertex verts[6] = {
            {strip.x0, 15.5f, 0, 1, 0xffffffffu, 0.2f, 1.f / 30.f},
            {strip.x1, 15.5f, 0, 1, 0xffffffffu, 0.8f, 1.f / 30.f},
            {strip.x0, 463.5f, 0, 1, 0xffffffffu, 0.2f, 29.f / 30.f},
            {strip.x1, 15.5f, 0, 1, 0xffffffffu, 0.8f, 1.f / 30.f},
            {strip.x0, 463.5f, 0, 1, 0xffffffffu, 0.2f, 29.f / 30.f},
            {strip.x1, 463.5f, 0, 1, 0xffffffffu, 0.8f, 29.f / 30.f},
        };
        draw_capture->primitives(touhou::graphics::Topology::Triangles, 2, verts, sizeof(Vertex));
    }
}

}  // namespace th13
