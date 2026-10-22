// msg 对话系统单测（S3 第一阶段）：MSG13 头/表解析、0x42a340 文本解密、
// cp932→UTF-8、指令推进/时间门、opcode 21 终局回调、等输入与两行配对语义；
// 尾部对真实 st01a.msg 的字节级用例（资产缺席时跳过）。
#include "../harness/test_framework.hpp"

#include "game/MsgPlayer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace th13;

namespace {

// 0x42a340 的逆：明文（含 NUL 终止）→ 密文字节。
std::vector<u8> encrypt_text(const std::string& plain) {
    std::vector<u8> out;
    u32 key = 0x77, inc = 0x07;
    for (char ch : plain) {
        out.push_back(u8(u8(ch) ^ key));
        key = (key + inc) & 0xFF;
        inc = (inc + 0x10) & 0xFF;
    }
    out.push_back(u8(key));  // NUL 的密文位
    return out;
}

struct InstructionBuilder {
    std::vector<u8> bytes;
    void add(u16 time, u8 opcode, const std::vector<u8>& args = {}) {
        bytes.push_back(u8(time & 0xFF));
        bytes.push_back(u8(time >> 8));
        bytes.push_back(opcode);
        bytes.push_back(u8(args.size()));
        bytes.insert(bytes.end(), args.begin(), args.end());
    }
    void text(u16 time, const std::string& plain) { add(time, 0x11, encrypt_text(plain)); }
    void integer(u16 time, u8 opcode, i32 value) {
        std::vector<u8> args(4);
        std::memcpy(args.data(), &value, 4);
        add(time, opcode, args);
    }
    void floats(u16 time, u8 opcode, float a, float b) {
        std::vector<u8> args(8);
        std::memcpy(args.data(), &a, 4);
        std::memcpy(args.data() + 4, &b, 4);
        add(time, opcode, args);
    }
    void end(u16 time) {
        add(time, 0);
        bytes.insert(bytes.end(), 4, 0);  // entry 尾 4 零字节填充（staff.msg 实证）
    }
};

void append_u32(std::vector<u8>& out, u32 value) {
    for (u32 i = 0; i < 4; i++) out.push_back(u8(value >> (i * 8)));
}

// 头（u32 count）+ 每项 8 字节表 {offset,id=0x100} + 指令流。
std::vector<u8> build_msg(const std::vector<std::vector<u8>>& entries) {
    std::vector<u8> out;
    append_u32(out, u32(entries.size()));
    u32 offset = 4 + u32(entries.size()) * 8;
    std::vector<u32> offsets;
    for (const auto& entry : entries) {
        offsets.push_back(offset);
        offset += u32(entry.size());
    }
    for (u32 value : offsets) {
        append_u32(out, value);
        append_u32(out, 0x100);
    }
    for (const auto& entry : entries) out.insert(out.end(), entry.begin(), entry.end());
    return out;
}

struct RecordHost final : MsgHost {
    std::vector<i32> bgm_calls, clears, boss_calls;
    i32 portraits = 0;
    void msg_bgm(i32 opcode) override { bgm_calls.push_back(opcode); }
    void msg_portrait_load() override { portraits++; }
    void msg_boss_battle() override { boss_calls.push_back(1); }
    void msg_stage_clear() override { clears.push_back(1); }
};

}  // namespace

TEST(msg_header_and_entry_table) {
    InstructionBuilder first, second;
    first.add(0, 0x01);
    first.end(10);
    second.add(0, 0x02);
    second.end(20);
    const auto bytes = build_msg({first.bytes, second.bytes});
    MsgFile file;
    std::string error;
    CHECK(file.open(bytes.data(), u32(bytes.size()), error));
    CHECK_EQ(file.entry_count(), 2u);
    CHECK_EQ(file.entry(0).offset, 4 + 2 * 8);  // 指令流起点 = 4+count*8
    CHECK_EQ(file.entry(0).id, 0x100u);
    CHECK_EQ(file.entry(1).offset, u32(4 + 2 * 8 + first.bytes.size()));
    CHECK_EQ(file.entry(1).id, 0x100u);
}

TEST(msg_header_rejects_truncated) {
    std::vector<u8> bad{2, 0, 0, 0, 0x40, 0, 0, 0};  // 声称 2 项但表不完整
    MsgFile file;
    std::string error;
    CHECK(!file.open(bad.data(), u32(bad.size()), error));
}

TEST(msg_decrypt_roundtrip) {
    // 0x42a340：key=0x77 起步、key+=inc、inc+=0x10（u8 环绕）。
    const auto encrypted = encrypt_text("Hello!");
    const auto decrypted = msg_decrypt_text(encrypted.data(), u32(encrypted.size()));
    CHECK_EQ(decrypted, std::string("Hello!\0", 7));  // 含 NUL 终止
    const auto japanese = encrypt_text(std::string("\x97\x48\x81\x58\x8E\x71\x97\x6C", 8));
    const auto decoded = msg_decrypt_text(japanese.data(), u32(japanese.size()));
    CHECK_EQ(decoded, std::string("\x97\x48\x81\x58\x8E\x71\x97\x6C"
                                  "\0",
                                  9));
}

TEST(cp932_to_utf8_samples) {
    // 幽々子様（cp932 97 48 81 58 8E 71 97 6C——st01a 对话数据实证用字）
    const u8 yuyuko[] = {0x97, 0x48, 0x81, 0x58, 0x8E, 0x71, 0x97, 0x6C};
    CHECK_EQ(cp932_to_utf8(yuyuko, sizeof yuyuko), std::string(u8"幽々子様"));
    // 半角片假名 ｱ→U+FF71、ASCII 直通
    const u8 half[] = {0xB1, 'A', 'z'};
    CHECK_EQ(cp932_to_utf8(half, sizeof half), std::string(u8"ｱAz"));
    // さーて、…（st01a entry0 首句）
    const u8 first_line[] = {0x82, 0xB3, 0x81, 0x5B, 0x82, 0xC4, 0x81, 0x41,
                             0x97, 0x48, 0x97, 0xEC};
    CHECK_EQ(cp932_to_utf8(first_line, sizeof first_line), std::string(u8"さーて、幽霊"));
}

TEST(msg_opcode21_fires_at_scheduled_clock) {
    InstructionBuilder entry;
    entry.integer(0, 0x0a, 1);  // 10;1 自动推进
    entry.add(0, 0x07);         // 左侧说话人
    entry.floats(0, 0x1c, 112.f, 288.f);
    entry.text(0, "First line");
    entry.integer(10, 0x0b, 600);  // 11;600 等输入
    entry.add(30, 0x15);           // 21 ★关终了
    entry.add(31, 0x16);           // 22 Boss 战设置
    entry.integer(40, 0x0a, 0);
    entry.end(50);
    const auto bytes = build_msg({entry.bytes});
    MsgFile file;
    std::string error;
    CHECK(file.open(bytes.data(), u32(bytes.size()), error));
    RecordHost host;
    MsgPlayer player;
    CHECK(player.open(file, 0, host, error));
    // 前 31 帧不含 clock=30（0x428ae0 时间门：clock >= time 执行；第 k 帧先
    // 执行后 +1，故 time=30 在第 31 次 step 命中）。11 的等待自动模式放行。
    for (int frame = 0; frame < 30; frame++) CHECK(player.step(false));
    CHECK_EQ(host.clears.size(), 0u);
    CHECK(player.step(false));
    CHECK_EQ(host.clears.size(), 1u);
    CHECK_EQ(host.boss_calls.size(), 0u);  // 22 在 time=31，尚未到
    CHECK(player.step(false));
    CHECK_EQ(host.boss_calls.size(), 1u);
    // 终止指令（time=50）后 done 且不再活跃。
    while (player.step(false)) {
    }
    CHECK(!player.active());
    CHECK(player.done());
    CHECK_EQ(host.clears.size(), 1u);
}

TEST(msg_wait_blocks_until_z_or_auto) {
    InstructionBuilder entry;
    entry.text(0, "Line");
    entry.integer(0, 0x0b, 600);  // 手动模式：等 Z
    entry.text(100, "After");
    entry.end(110);
    const auto bytes = build_msg({entry.bytes});
    MsgFile file;
    std::string error;
    CHECK(file.open(bytes.data(), u32(bytes.size()), error));
    RecordHost host;
    MsgPlayer player;
    CHECK(player.open(file, 0, host, error));
    player.step(false);  // 首句入块，停在 11
    for (int frame = 0; frame < 60; frame++) player.step(false);
    CHECK(player.active());                  // 无 Z：仍阻塞
    CHECK_EQ(player.state().lines.size(), 1u);
    CHECK(player.step(true));                // Z 边沿放行：行块清、指针前进
    CHECK_EQ(player.state().lines.size(), 0u);
    for (int frame = 0; frame < 40; frame++) player.step(false);  // 时钟至 100
    CHECK_EQ(player.state().lines.size(), 1u);  // time=100 的次句入块
    CHECK_EQ(player.state().lines[0], std::string("After"));
}

TEST(msg_auto_advance_threshold) {
    InstructionBuilder entry;
    entry.integer(0, 0x0a, 1);  // 10;1 → 自动推进
    entry.text(0, "Auto");
    entry.integer(0, 0x0b, 600);
    entry.text(1, "Next");
    entry.end(10);
    const auto bytes = build_msg({entry.bytes});
    MsgFile file;
    std::string error;
    CHECK(file.open(bytes.data(), u32(bytes.size()), error));
    RecordHost host;
    MsgPlayer player;
    CHECK(player.open(file, 0, host, error));
    player.step(false);  // 开场帧：行入块 + 停在 11
    CHECK_EQ(player.state().lines.size(), 1u);
    // 自动模式阈值 0x14（0x42b3f0 <20 观察值【待校准】）：第 21 帧重入放行。
    for (int frame = 0; frame < 19; frame++) player.step(false);
    CHECK_EQ(player.state().lines.size(), 1u);
    player.step(false);
    CHECK_EQ(player.state().lines.size(), 1u);  // 通过后重入清块，Next 入块
    CHECK_EQ(player.state().lines[0], std::string("Next"));
}

TEST(msg_line_pairing_two_per_block) {
    InstructionBuilder entry;
    entry.add(0, 0x07);
    entry.text(0, "one");
    entry.text(0, "two");
    entry.text(0, "three");  // 无 11 间隔：第三行重开块（0x5f==0 && 0x60==0 清）
    entry.end(5);
    const auto bytes = build_msg({entry.bytes});
    MsgFile file;
    std::string error;
    CHECK(file.open(bytes.data(), u32(bytes.size()), error));
    RecordHost host;
    MsgPlayer player;
    CHECK(player.open(file, 0, host, error));
    player.step(false);
    CHECK_EQ(player.state().lines.size(), 1u);
    CHECK_EQ(player.state().lines[0], std::string("three"));
    CHECK_EQ(player.state().side, 0);
}

TEST(msg_side_and_box_position) {
    InstructionBuilder entry;
    entry.add(0, 0x08);  // 右侧说话人
    entry.floats(0, 0x1c, 336.f, 272.f);
    entry.text(0, "right");
    entry.end(5);
    const auto bytes = build_msg({entry.bytes});
    MsgFile file;
    std::string error;
    CHECK(file.open(bytes.data(), u32(bytes.size()), error));
    RecordHost host;
    MsgPlayer player;
    CHECK(player.open(file, 0, host, error));
    player.step(false);
    CHECK_EQ(player.state().side, 1);
    CHECK_EQ(player.state().box_x, 336.f);
    CHECK_EQ(player.state().box_y, 272.f);
}

TEST(msg_real_st01a_bytes) {
    // 真实资产字节级用例：repo 根相对 ../artifacts/reference/raw/st01a.msg
    //（主仓库布局；隔离子仓验证可用 TH13_RAW_DIR 覆盖）。缺席即跳过。
    std::vector<std::string> candidates;
    if (const char* override_dir = std::getenv("TH13_RAW_DIR"))
        candidates.push_back(std::string(override_dir) + "/st01a.msg");
    candidates.push_back("../artifacts/reference/raw/st01a.msg");
    candidates.push_back("../../../artifacts/reference/raw/st01a.msg");
    FILE* asset = nullptr;
    for (const auto& path : candidates) {
        asset = std::fopen(path.c_str(), "rb");
        if (asset) break;
    }
    if (!asset) {
        std::printf("# 跳过：st01a.msg 不在本机（相对 repo 根）\n");
        return;
    }
    std::vector<u8> bytes;
    u8 buffer[4096];
    size_t got;
    while ((got = std::fread(buffer, 1, sizeof buffer, asset)) > 0)
        bytes.insert(bytes.end(), buffer, buffer + got);
    std::fclose(asset);
    MsgFile file;
    std::string error;
    CHECK(file.open(bytes.data(), u32(bytes.size()), error));
    CHECK_EQ(file.entry_count(), 2u);           // entry0=Boss 前 / entry1=Boss 后
    CHECK_EQ(file.entry(1).offset, 0x334u);     // 头表字节实证
    CHECK_EQ(file.entry(0).offset, 0x14u);
    // 逐 entry 扫 opcode：只有 entry 1 带 21（全 28 个 st0N[a-d] 各恰一处）。
    i32 clears[2] = {0, 0};
    i32 texts_entry0 = 0;
    std::string first_text;
    for (u32 index = 0; index < 2; index++) {
        const u8* cursor = file.bytes() + file.entry(index).offset;
        while (cursor + 4 <= file.bytes() + file.size()) {
            u16 time = 0;
            std::memcpy(&time, cursor, 2);
            const u8 opcode = cursor[2];
            const u8 arg_bytes = cursor[3];
            if (opcode == 0) break;
            if (opcode == 0x15) clears[index]++;
            if (opcode == 0x11 && index == 0) {
                texts_entry0++;
                if (first_text.empty())
                    first_text = cp932_to_utf8(
                        reinterpret_cast<const u8*>(msg_decrypt_text(cursor + 4, arg_bytes).data()),
                        u32(msg_decrypt_text(cursor + 4, arg_bytes).size()));
            }
            cursor += 4 + arg_bytes;
        }
    }
    CHECK_EQ(clears[0], 0);
    CHECK_EQ(clears[1], 1);
    CHECK(texts_entry0 >= 10);
    CHECK_EQ(first_text, std::string(u8"さーて、幽霊の親玉を倒さないとね！"));
}

int main() { return th13test::run_all("msg 对话系统"); }
