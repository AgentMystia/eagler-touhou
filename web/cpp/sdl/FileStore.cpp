// FileStore 实现：v1 定长小端布局（36 字节）：
//   "TH13SAVE" | u32 version=1 | u32 cleared_mask | i32 bgm | i32 effect | u32[3] 保留
// 读档：magic/版本/尺寸不符 = 默认值（原版坏 scoreth13.dat 语义，不报错）。
// 写档：任何 I/O 失败 = false + error。web 走 localStorage（EM_JS try/catch），
// native/WASI 走真实文件；两侧同一序列化，互不伪造成功。
#include "FileStore.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// localStorage 持久化（宿主无文件系统持久卷）：base64 文本。任何异常/限额
// 都返回 false —— 失败可见，不吞。
EM_JS(bool, th13_save_write_js, (const char* data, int length), {
    try {
        let binary = '';
        const bytes = new Uint8Array(HEAPU8.buffer, data, length);
        for (let i = 0; i < length; ++i) binary += String.fromCharCode(bytes[i]);
        localStorage.setItem('th13.save', btoa(binary));
        return true;
    } catch (e) {
        return false;
    }
});
EM_JS(int, th13_save_read_js, (char* out, int capacity), {
    try {
        const text = localStorage.getItem('th13.save');
        if (text === null || text === '') return 0;
        const binary = atob(text);
        if (binary.length > capacity) return 0;
        const bytes = new Uint8Array(HEAPU8.buffer, out, binary.length);
        for (let i = 0; i < binary.length; ++i) bytes[i] = binary.charCodeAt(i);
        return binary.length;
    } catch (e) {
        // 读侧异常按缺档处理（原版读不到 score.dat = 默认值照常开局）；
        // 持久化失败在写侧（save）如实暴露，不在此阻断启动。
        return 0;
    }
});
#endif

namespace th13::sdl {
namespace {
constexpr char kSaveMagic[8] = {'T', 'H', '1', '3', 'S', 'A', 'V', 'E'};
constexpr u32 kSaveVersion = 1;
constexpr size_t kSaveBytes = 8 + 4 * 2 + 4 * 2 + 4 * 3;  // 36

void put32(u8* at, u32 value) { std::memcpy(at, &value, 4); }
u32 get32(const u8* at) {
    u32 value = 0;
    std::memcpy(&value, at, 4);
    return value;
}
}  // namespace

bool FileStore::cleared(i32 character, i32 difficulty) const {
    if (character < 0 || character >= kSaveCharacters || difficulty < 0 ||
        difficulty >= kSaveDifficulties)
        return false;
    return (cleared_mask >> u32(character * kSaveDifficulties + difficulty)) & 1u;
}

void FileStore::set_cleared(i32 character, i32 difficulty) {
    if (character < 0 || character >= kSaveCharacters || difficulty < 0 ||
        difficulty >= kSaveDifficulties)
        return;
    cleared_mask |= 1u << u32(character * kSaveDifficulties + difficulty);
}

bool FileStore::initialize() {
    error.clear();
    loaded = false;
    std::array<u8, kSaveBytes> buffer{};
#ifdef __EMSCRIPTEN__
    const int length = th13_save_read_js(reinterpret_cast<char*>(buffer.data()),
                                         int(buffer.size()));
    if (length == 0) return true;         // 缺档/读异常 = 新档默认值（原版语义）
    if (size_t(length) != buffer.size()) return true;  // 尺寸不符 = 默认值
#else
    const char* override_path = std::getenv("TH13_SAVE_PATH");
    const std::string path = override_path ? override_path : "th13-save.bin";
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return true;  // 缺档 = 新档默认值（原版语义，非错误）
    const size_t length = std::fread(buffer.data(), 1, buffer.size(), file);
    const bool failed = std::ferror(file) != 0;
    std::fclose(file);
    if (failed) {
        error = "Save file read failed: " + path;
        return false;
    }
    if (length != buffer.size()) return true;  // 尺寸不符 = 默认值
#endif
    if (std::memcmp(buffer.data(), kSaveMagic, sizeof(kSaveMagic)) != 0 ||
        get32(buffer.data() + 8) != kSaveVersion)
        return true;  // 坏档 = 默认值（原版语义）
    cleared_mask = get32(buffer.data() + 12);
    bgm_volume = i32(get32(buffer.data() + 16));
    effect_volume = i32(get32(buffer.data() + 20));
    heard_mask = get32(buffer.data() + 24) & 0xffu;  // reserved[0]：31 曲位
    if (bgm_volume < 0 || bgm_volume > 100) bgm_volume = 100;
    if (effect_volume < 0 || effect_volume > 100) effect_volume = 80;
    loaded = true;
    return true;
}

bool FileStore::save() {
    error.clear();
    std::array<u8, kSaveBytes> buffer{};
    std::memcpy(buffer.data(), kSaveMagic, sizeof(kSaveMagic));
    put32(buffer.data() + 8, kSaveVersion);
    put32(buffer.data() + 12, cleared_mask);
    put32(buffer.data() + 16, u32(bgm_volume));
    put32(buffer.data() + 20, u32(effect_volume));
    put32(buffer.data() + 24, heard_mask & 0xffu);  // reserved[0]（上限 31 曲）
#ifdef __EMSCRIPTEN__
    if (!th13_save_write_js(reinterpret_cast<const char*>(buffer.data()),
                            int(buffer.size()))) {
        error = "Save storage write failed";
        return false;
    }
    return true;
#else
    const char* override_path = std::getenv("TH13_SAVE_PATH");
    const std::string path = override_path ? override_path : "th13-save.bin";
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        error = "Save file open failed: " + path;
        return false;
    }
    const bool ok = std::fwrite(buffer.data(), 1, buffer.size(), file) == buffer.size();
    std::fclose(file);
    if (!ok) error = "Save file write failed: " + path;
    return ok;
#endif
}

}  // namespace th13::sdl
