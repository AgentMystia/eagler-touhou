// STD 舞台背景场景：图元 VM 池（st01wl.anm 脚本）+ 相机脚本驱动 + 实例
// （FACE）遍历绘制（裁剪/雾/深度写）。数据通路对齐 th15 StageScene 的 th13
// 同族裁剪：不含 th15 专属的形变/嵌入动画服务（st01 窗口外，接入即显式报错）。
#pragma once
#include "AnmManager.hpp"
#include "AnmRenderer.hpp"
#include "StageResource.hpp"
#include "StageScript.hpp"
#include "Timer.hpp"
#include <vector>
namespace th13 {
class StageScenery {
public:
    StageResource file;
    StageScript script{file};
    std::string error;
    u32 frames = 0;
    bool ready() const noexcept { return initialized; }
    const StageCamera& camera() const noexcept { return script.state.camera; }
    const Vec3& camera_direction() const noexcept { return view_direction; }
    bool initialize(AnmManager& animations, i32 bank);
    bool update(AnmManager& animations, AnmEnvironment& environment, float rate = 1);
    // layer_first..layer_last 的对象组绘制（背景 0-7 / 前景 8-11 两段调用）；
    // cull_extent = 裁剪矩形尺寸（原点取视口左上）。
    bool draw(AnmManager& animations, AnmRenderer& renderer, ZunGraphics& graphics,
              i32 layer_first, i32 layer_last, const GraphicsViewport& viewport, Vec2 cull_extent);
    bool interrupt_objects(i32 label);
private:
    std::vector<AnmVm> primitives;
    std::vector<u8> object_flags;
    std::vector<i16> instance_flags;
    bool initialized = false;
    float rate = 1;
    Vec3 view_direction{};
    bool bind(AnmVm&, AnmManager&, i32 bank, i32 script_index);
    bool fail(const std::string& message) {
        if (error.empty()) error = message;
        return false;
    }
};
}  // namespace th13
