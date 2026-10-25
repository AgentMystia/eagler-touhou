// Real native serialization regression for Practice stage-played data.
#include "../harness/test_framework.hpp"
// tests/ui/run.sh deliberately links only game/*.cpp, so include this isolated,
// platform-free storage implementation here rather than changing the shared runner.
#include "../../web/cpp/sdl/FileStore.cpp"
#include <array>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

using namespace th13;
using namespace th13::sdl;
namespace {
std::string fixture_path() {
    static int next = 0;
    const char* directory = std::getenv("TH13_TEST_OUTPUT");
    if (directory) {
        // 测试指定的输出目录可能不存在：自建，不因 fopen 失败误报套件红。
        std::filesystem::create_directories(directory);
    }
    const std::string path = std::string(directory ? directory : "/tmp") +
        "/practice-save-" + std::to_string(getpid()) + "-" + std::to_string(next++) + ".bin";
    CHECK_EQ(setenv("TH13_SAVE_PATH", path.c_str(), 1), 0);
    return path;
}
void put32(std::vector<u8>& bytes, size_t at, u32 value) {
    std::memcpy(bytes.data() + at, &value, 4);
}
void write_fixture(const std::string& path, const std::vector<u8>& bytes) {
    FILE* file = std::fopen(path.c_str(), "wb");
    CHECK(file != nullptr);
    if (!file) return;
    CHECK_EQ(std::fwrite(bytes.data(), 1, bytes.size(), file), bytes.size());
    CHECK_EQ(std::fclose(file), 0);
}
std::vector<u8> legacy_save() {
    std::vector<u8> bytes(36);
    std::memcpy(bytes.data(), "TH13SAVE", 8);
    put32(bytes, 8, 1);
    put32(bytes, 12, 1u << 7);
    put32(bytes, 16, 41);
    put32(bytes, 20, 63);
    put32(bytes, 24, 0x85);
    return bytes;
}
}

TEST(practice_missing_save_does_not_invent_stage1_unlock) {
    fixture_path();
    FileStore files;
    CHECK(files.initialize());
    CHECK(!files.loaded);
    for (i32 c = 0; c < 4; ++c)
        for (i32 d = 0; d < 4; ++d)
            for (i32 s = 1; s <= 6; ++s) CHECK(!files.practice_stage_played(c, d, s));
}

TEST(practice_v1_preserves_settings_and_clears_unknown_played_data) {
    const std::string path = fixture_path();
    write_fixture(path, legacy_save());
    FileStore files;
    files.mark_practice_stage_played(0, 0, 1);  // initialize must clear stale instance state
    CHECK(files.initialize());
    CHECK(files.loaded);
    CHECK_EQ(files.bgm_volume, 41);
    CHECK_EQ(files.effect_volume, 63);
    CHECK_EQ(files.cleared_mask, 1u << 7);
    CHECK_EQ(files.heard_mask, 0x85u);
    for (u8 played : files.practice_played) CHECK_EQ(played, u8(0));
    files.mark_practice_stage_played(3, 2, 6);
    CHECK(files.save());
    FileStore roundtrip;
    CHECK(roundtrip.initialize());
    CHECK(roundtrip.loaded);
    CHECK(roundtrip.practice_stage_played(3, 2, 6));
    CHECK_EQ(roundtrip.cleared_mask, 1u << 7);
    CHECK_EQ(roundtrip.bgm_volume, 41);
    CHECK_EQ(roundtrip.heard_mask, 0x85u);
}

TEST(practice_v2_roundtrips_all_stage_character_and_rank_dimensions) {
    fixture_path();
    FileStore files;
    for (i32 c = 0; c < 4; ++c)
        for (i32 d = 0; d < 4; ++d)
            files.mark_practice_stage_played(c, d, (c + d) % 6 + 1);
    CHECK(files.save());
    FileStore read;
    CHECK(read.initialize());
    CHECK(read.loaded);
    for (i32 c = 0; c < 4; ++c)
        for (i32 d = 0; d < 4; ++d)
            for (i32 s = 1; s <= 6; ++s)
                CHECK_EQ(read.practice_stage_played(c, d, s), s == (c + d) % 6 + 1);
    const auto before = read.practice_played;
    for (const std::array<i32, 3>& bad : {std::array<i32, 3>{-1, 0, 1}, {4, 0, 1},
                                         {0, -1, 1}, {0, 4, 1}, {0, 0, 0}, {0, 0, 7}}) {
        CHECK(!read.practice_stage_played(bad[0], bad[1], bad[2]));
        read.mark_practice_stage_played(bad[0], bad[1], bad[2]);
    }
    CHECK(read.practice_played == before);
}

TEST(practice_corrupt_truncated_or_overlong_saves_revert_to_defaults) {
    std::vector<std::vector<u8>> invalid;
    auto bytes = legacy_save();
    bytes[0] = 'X'; invalid.push_back(bytes);
    bytes = legacy_save(); put32(bytes, 8, 2); invalid.push_back(bytes);  // v2 truncated to v1 size
    bytes = legacy_save(); bytes.resize(35); invalid.push_back(bytes);
    bytes = legacy_save(); bytes.resize(52); invalid.push_back(bytes);  // v1 wrong size
    bytes = legacy_save(); put32(bytes, 8, 2); bytes.resize(53); invalid.push_back(bytes);
    for (const auto& corrupt : invalid) {
        const std::string path = fixture_path();
        write_fixture(path, corrupt);
        FileStore files;
        files.cleared_mask = 7;
        files.bgm_volume = 1;
        files.mark_practice_stage_played(0, 0, 1);
        CHECK(files.initialize());
        CHECK(!files.loaded);
        CHECK_EQ(files.cleared_mask, 0u);
        CHECK_EQ(files.bgm_volume, 100);
        for (u8 played : files.practice_played) CHECK_EQ(played, u8(0));
    }
}

int main() { return th13test::run_all("Practice save migration"); }
