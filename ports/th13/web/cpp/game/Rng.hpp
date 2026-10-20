// th13 随机数发生器——MSVC CRT rand() 行为镜像。
// 依据：th13.exe .text 中唯一一处 LCG 常量对（0x47BBAE/0x47BBB3，imul/add 相邻），
// 即静态链接的 CRT rand()；游戏侧调用路径与状态地址由 Ghidra xref 锁定后
// 在 AddressMap.hpp 登记，并由 Unicorn oracle 逐值验证。
#pragma once

#include <cstdint>

namespace th13 {

class Rng {
public:
    explicit Rng(uint32_t seed = 1)
        : state_(seed) {}

    void reseed(uint32_t seed) { state_ = seed; }

    // MSVC rand()：state = state * 0x343FD + 0x269EC3；返回 (state >> 16) & 0x7FFF
    int next() {
        state_ = state_ * kMultiplier + kIncrement;
        return static_cast<int>((state_ >> 16) & 0x7FFF);
    }

    // 供 oracle 比对：不推进状态的观察
    uint32_t state() const { return state_; }

    static constexpr uint32_t kMultiplier = 0x343FDu;
    static constexpr uint32_t kIncrement = 0x269EC3u;

private:
    uint32_t state_;
};

}  // namespace th13
