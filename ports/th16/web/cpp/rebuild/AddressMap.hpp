// 原版 th16.exe（v1.00a，PE 时间戳 2017-07-31，SHA-256 c1177601…1929）地址锚点表。
// 每完成一个子系统的行为还原，就在此登记它的原版入口与 C++ 对应物，
// 使 oracle 对照与回归脚本可以直接引用，不再散落硬编码地址。
// 反编译证据：artifacts/th16/decomp/jp/functions/<地址>.{c,asm}（工作区忽略目录）。
#pragma once

#include <cstdint>

namespace th16::rebuild {

constexpr uint32_t kImageBase = 0x400000;
constexpr uint32_t kEntryPoint = 0x47537B;  // PE 头入口（CRT 启动）
constexpr uint32_t kCrtBoundary = 0x470000;  // 以上为静态链接 MSVC CRT（1006 游戏函数 / 755 CRT 函数）

struct AddressAnchor {
    const char* subsystem;
    uint32_t originalEntry;
    const char* cpp;  // C++ 对应物（nullptr = 未还原）
};

constexpr AddressAnchor kAnchors[] = {
    // —— 已还原（本轮）——
    {"rng-step16", 0x402B70, "game/Rng.hpp Rng::next16"},
    {"rng-step32", 0x402BE0, "game/Rng.hpp Rng::next32"},
    {"rng-unit", 0x402C70, "game/Rng.hpp Rng::unit"},
    {"rng-signed-unit", 0x402CB0, "game/Rng.hpp Rng::signed_unit"},
    {"rng-angle", 0x402CF0, "game/Rng.hpp Rng::angle"},
    {"resource-crypt", 0x402220, "game/ResourceCrypt.cpp resource_crypt"},
    {"archive-open", 0x4572E0, "game/Archive.cpp Archive::open"},
    {"archive-directory", 0x4574B0, "game/Archive.cpp Archive::open（目录项）"},
    {"archive-read-entry", 0x457120, "game/Archive.cpp Archive::read"},
    {"archive-lzss", 0x457F00, "game/Lzss.cpp Lzss::decode"},
    {"anm-bank-load", 0x46CDD0, "game/AnmResource.cpp AnmResource::parse（格式面）"},
    {"anm-entry-check", 0x46D0C0, "game/AnmResource.cpp（version==8 / hasdata / '@'）"},
    {"anm-bank-slot", 0x46CF80, nullptr},  // 31 个 bank 槽（param_1 ≤ 0x1e），对象 0x13c 字节
    // —— 已定位，待还原 ——
    {"ecl-dispatch", 0x41DCB0, nullptr},     // 19976 字节、81 个被调、switch 密集：ECL 指令主分发
    {"anm-vm-update", 0x45F980, nullptr},    // 13109 字节、64 个调用方：ANM VM 指令解释
    {"title-scene-init", 0x44AC70, nullptr}, // 装载 title.anm(bank 0x10) / title_v.anm(bank 0x11)
    {"boot-scene-init", 0x43ADC0, "sdl/Application.cpp"},  // sig.anm → text.anm → thbgm.fmt → thbgm.dat → score；静态首帧已上屏，VM 执行待 anm-vm-update
    {"anm-load-file", 0x46D020, nullptr},    // AnmManager::load_file(slot, name)：从档案读 .anm 并进 bank
    {"score-file", 0x4497E0, nullptr},       // scoreth16.dat 读写
    {"replay-file", 0x4403C5, nullptr},      // th16_%.2d.rpy
    {"ver-file", 0x43B480, nullptr},         // th16_%.4x%c.ver
    {"music-fmt", 0x43AEAE, nullptr},        // ../../bgm/thbgm.fmt
    // —— 未定位 ——
    {"supervisor", 0, nullptr},
    {"enemy-manager", 0, nullptr},
    {"bullet-manager", 0, nullptr},
    {"item-manager", 0, nullptr},
    {"player", 0, nullptr},
    {"season-system", 0, nullptr},  // th16 特有：季节槽/季节解放
};

}  // namespace th16::rebuild
