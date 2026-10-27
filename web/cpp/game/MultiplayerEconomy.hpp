#pragma once
#include "PlayerLife.hpp"
#include "MultiplayerChapterReward.hpp"
#include <array>
namespace th15 {
constexpr bool multiplayer_can_give_power(i32 power,i32 step)noexcept{return step>0&&power>=step&&power-step>=step;}
}
