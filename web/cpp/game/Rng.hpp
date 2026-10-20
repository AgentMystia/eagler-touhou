// th13 随机数发生器——行为以原版 Ghidra 反编译与 Unicorn oracle 对照锁定。
// 常量在对照闭环中固化；当前值为待验证占位（见 docs/DEVELOPMENT.md）。
#pragma once

#include <cstdint>

namespace th13 {

class Rng {
public:
    explicit Rng(uint32_t seed = 0)
        : state_(seed) {}

    void reseed(uint32_t seed) { state_ = seed; }

    uint32_t next() {
        state_ = state_ * kMultiplier + kIncrement;
        return state_;
    }

    // 供调试导出/比对：不推进状态的观察
    uint32_t state() const { return state_; }

    static constexpr uint32_t kMultiplier = 0x41C64E6Du;
    static constexpr uint32_t kIncrement = 0x3039u;

private:
    uint32_t state_;
};

}  // namespace th13
