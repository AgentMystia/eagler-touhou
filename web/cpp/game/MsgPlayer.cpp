#include "MsgPlayer.hpp"

#include "MsgPlayerCp932.hpp"

#include <cstring>

namespace th13 {

namespace {

bool read_u32(const std::vector<u8>& bytes, u32 at, u32& value) {
    if (at + 4 > bytes.size()) return false;
    std::memcpy(&value, bytes.data() + at, 4);
    return true;
}

void append_utf8(std::string& out, u32 code) {
    if (code < 0x80) {
        out.push_back(char(code));
    } else if (code < 0x800) {
        out.push_back(char(0xC0 | (code >> 6)));
        out.push_back(char(0x80 | (code & 0x3F)));
    } else if (code < 0x10000) {
        out.push_back(char(0xE0 | (code >> 12)));
        out.push_back(char(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(char(0x80 | (code & 0x3F)));
    } else {
        out.push_back(char(0xF0 | (code >> 18)));
        out.push_back(char(0x80 | ((code >> 12) & 0x3F)));
        out.push_back(char(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(char(0x80 | (code & 0x3F)));
    }
}

}  // namespace

bool MsgFile::open(const u8* data, u32 size, std::string& error) {
    entries_.clear();
    bytes_.assign(data, data + size);
    u32 count = 0;
    if (size < 4 || !read_u32(bytes_, 0, count) || count == 0 || count > 64) {
        error = "msg header entry count invalid";
        return false;
    }
    const u32 table_bytes = 4 + count * 8;  // 头 + 表（指令流起点）
    if (size < table_bytes) {
        error = "msg entry table truncated";
        return false;
    }
    for (u32 index = 0; index < count; index++) {
        MsgEntry entry;
        if (!read_u32(bytes_, 4 + index * 8, entry.offset) ||
            !read_u32(bytes_, 4 + index * 8 + 4, entry.id)) {
            error = "msg entry header read failed";
            return false;
        }
        if (entry.offset < table_bytes || entry.offset >= size) {
            error = "msg entry offset out of range";
            return false;
        }
        entries_.push_back(entry);
    }
    return true;
}

std::string msg_decrypt_text(const u8* args, u32 arg_bytes) {
    // FUN_0042a340：key=0x77 起，每字节 key+=inc、inc+=0x10（u8 环绕），
    // 解出 NUL 即止（加密串以「异或后为 0」的字节收尾）。
    std::string out;
    out.reserve(arg_bytes);
    u32 key = 0x77;
    u32 inc = 0x07;
    for (u32 index = 0; index < arg_bytes; index++) {
        const u8 decoded = u8(args[index] ^ key);
        out.push_back(char(decoded));
        if (decoded == 0) break;
        key = (key + inc) & 0xFF;
        inc = (inc + 0x10) & 0xFF;
    }
    return out;
}

std::string cp932_to_utf8(const u8* data, u32 size) {
    std::string out;
    out.reserve(size);
    u32 index = 0;
    while (index < size) {
        const u8 lead = data[index];
        if (lead == 0) break;  // NUL 收尾
        if (lead < 0x80) {
            append_utf8(out, lead);
            index++;
            continue;
        }
        if (lead >= 0xA1 && lead <= 0xDF) {  // 半角片假名 → U+FF61..
            append_utf8(out, 0xFF61u + (lead - 0xA1));
            index++;
            continue;
        }
        if (lead < 0x81 || lead > 0xEF || lead >= 0xF0 || index + 1 >= size) {
            append_utf8(out, 0xFFFD);
            index++;
            continue;
        }
        const u8 row = kCp932Row[lead - 0x81];
        const u8 trail = data[index + 1];
        u32 code = 0xFFFD;
        if (row != 0xFF && trail >= kCp932TrailBase &&
            trail < kCp932TrailBase + kCp932TrailCols) {
            const u16 mapped = kCp932ToUnicode[row][trail - kCp932TrailBase];
            if (mapped != 0) code = mapped;
        }
        append_utf8(out, code);
        index += code == 0xFFFD ? 1 : 2;
    }
    return out;
}

bool MsgPlayer::open(MsgFile& file, u32 entry_index, MsgHost& host, std::string& error) {
    if (entry_index >= file.entry_count()) {
        error = "msg entry index out of range";
        return false;
    }
    file_ = &file;
    host_ = &host;
    cursor_ = file.bytes() + file.entry(entry_index).offset;
    state_ = MsgDialogState{};
    state_.active = true;
    state_.entry = i32(entry_index);
    clock_ = 0;
    line_index_ = 0;
    block_open_ = 0;
    wait_frames_ = 0;
    frame_pause_ = false;
    return true;
}

void MsgPlayer::finish() {
    // 0x4265a0：执行器返回非 0 → 当帧销毁对话（+0x198=0），519 下一帧放行。
    state_.done = true;
    state_.active = false;
}

bool MsgPlayer::step(bool z_pressed) {
    if (!state_.active) return false;
    frame_pause_ = false;
    for (;;) {
        if (cursor_ + 4 > file_->bytes() + file_->size()) {
            finish();  // 越界按终止处理（不应发生：表偏移经 open 校验）
            return false;
        }
        u16 time = 0;
        std::memcpy(&time, cursor_, 2);
        const u8 opcode = cursor_[2];
        const u8 arg_bytes = cursor_[3];
        if (cursor_ + 4 + arg_bytes > file_->bytes() + file_->size()) {
            finish();
            return false;
        }
        if (opcode == 0) {  // case 0x0：entry 终止
            finish();
            return false;
        }
        if (clock_ < time) break;  // 时间门（0x428ae0:761）
        const Flow flow = execute(opcode, cursor_ + 4, arg_bytes, z_pressed);
        if (flow == Flow::Blocked) break;  // 指针停在阻塞指令上（如 opcode 11）
        cursor_ += 4 + arg_bytes;
        if (frame_pause_) break;  // opcode 12：本帧止步（原 +0x174=1 门）
    }
    if (state_.active) clock_++;  // 0x4265a0 rate=1 路径
    return state_.active;
}

MsgPlayer::Flow MsgPlayer::execute(u8 opcode, const u8* args, u32 arg_bytes,
                                   bool z_pressed) {
    switch (opcode) {
        case 0x01:  // 1：按角色装载立绘 bank（0x4c0aa0 + be7b8*4）
            host_->msg_portrait_load();
            return Flow::Advance;
        case 0x02:  // 2：BGM（stage 表 0x64 门的两变体）
        case 0x13:  // 19：0x43ae40(1, stage 表+0x2c)
        case 0x14:  // 20：Boss 曲（be824<5 难度门）
        case 0x1f:  // 31：按名播曲（stage 表+0x68）
            host_->msg_bgm(i32(opcode));
            return Flow::Advance;
        case 0x04:  // 4：释放左立绘槽（0x10）
        case 0x05:  // 5：释放右槽（0x11）+0x17
        case 0x06:  // 6：释放名牌四槽（0x13-0x16）+ 清文本区
        case 0x0d:  // 13：隐左立绘
        case 0x0e:  // 14：隐右立绘
        case 0x0c:  // 12：单帧止步（+0x5d=1）
            if (opcode == 0x0c) frame_pause_ = true;
            if (opcode == 0x06) state_.lines.clear();
            return Flow::Advance;
        case 0x07:  // 7：左侧说话人（0x61=0，行计数清）
        case 0x08:  // 8：右侧说话人（0x61=1）
            state_.side = opcode == 0x08 ? 1 : 0;
            line_index_ = 0;
            block_open_ = 0;
            return Flow::Advance;
        case 0x09:  // 9：双立绘名牌变体（0x54.. 位重排）——视觉 TODO
            return Flow::Advance;
        case 0x0a: {  // 10：自动推进位翻转（arg0 & 1）
            i32 value = 0;
            if (arg_bytes >= 4) std::memcpy(&value, args, 4);
            state_.auto_advance = (value & 1) != 0;
            return Flow::Advance;
        }
        case 0x0b: {  // 11：等输入（case 0xb 的阻塞语义）
            // 通过条件（0x428ae0:366-380）：文本已出全后，Z 边沿（0x5e|=0x40）
            // 或自动模式计数 >= 0x14（0x42b3f0 观察值；逐帧口径待 oracle 校准）。
            if (z_pressed || (state_.auto_advance && wait_frames_ >= 20)) {
                line_index_ = 0;  // case 0xb 通过路径重置行块
                block_open_ = 0;
                state_.lines.clear();
                wait_frames_ = 0;
                return Flow::Advance;
            }
            wait_frames_++;
            return Flow::Blocked;  // 指针不动，下一帧重入本指令
        }
        case 0x11: {  // 17：文本行（解密 + `|ruby` 前缀 + 两行/块配对）
            std::string decoded = msg_decrypt_text(args, arg_bytes);
            u32 begin = 0;
            // case 0x11:455-467：`|` 前缀 = 读音注记（|a,b,正文）；a/b 消费掉，
            // 注记渲染属字体阶段（TODO），正文照常入块。
            if (!decoded.empty() && decoded[0] == '|') {
                u32 commas = 0;
                u32 scan = 1;
                while (scan < decoded.size() && commas < 2) {
                    if (decoded[scan] == ',') commas++;
                    scan++;
                }
                begin = commas == 2 ? scan : 1;
            }
            const u8* raw = reinterpret_cast<const u8*>(decoded.data()) + begin;
            const std::string text = cp932_to_utf8(raw, u32(decoded.size() - begin));
            if (line_index_ == 0 && block_open_ == 0) state_.lines.clear();
            state_.lines.push_back(text);
            if (line_index_ == 0) {
                block_open_ = 1;
                line_index_ = 1;
            } else {
                line_index_ = 0;
                block_open_ = 0;
            }
            return Flow::Advance;
        }
        case 0x12:  // 18：释放槽位族（case 0x12）
        case 0x17:  // 23：释放槽 0x10
        case 0x18:  // 24：释放槽 0x11
        case 0x19:  // 25：名牌缩放四参（+0x4e4）——视觉 TODO
        case 0x1a:  // 26：显示位（0x5e |= 2）——视觉 TODO
        case 0x1e:  // 30：原版 switch 无 case（dump 18 处用例，空转）
            return Flow::Advance;
        case 0x15:  // 21：★关终了（case 0x15 → 0x42cd40，0x428ae0:673-675）
            host_->msg_stage_clear();
            return Flow::Advance;
        case 0x16:  // 22：Boss 战设置（0x43af70；stage==6/!=6 两支同调）
        case 0x1b:  // 27：同 0x43af70
            host_->msg_boss_battle();
            return Flow::Advance;
        case 0x1c: {  // 28：对话框 x,y（两 f32 位型存整槽，case 0x1c）
            float x = state_.box_x, y = state_.box_y;
            if (arg_bytes >= 4) std::memcpy(&x, args, 4);
            if (arg_bytes >= 8) std::memcpy(&y, args + 4, 4);
            state_.box_x = x;
            state_.box_y = y;
            return Flow::Advance;
        }
        case 0x1d: {  // 29：文字属性（(arg*4) 异或进 0x5e 的 bit2-5）
            i32 value = 0;
            if (arg_bytes >= 4) std::memcpy(&value, args, 4);
            state_.text_attr = (state_.text_attr & ~0x3Cu) | (u32(value) * 4 & 0x3Cu);
            return Flow::Advance;
        }
        default:
            // 未知 opcode：按 arg_bytes 步进不中断（与原版 switch 落空同效）。
            return Flow::Advance;
    }
}

}  // namespace th13
