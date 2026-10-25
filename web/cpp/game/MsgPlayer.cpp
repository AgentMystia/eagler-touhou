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

bool build_cp932_font_mapping(std::vector<u8>& out) {
    out.assign(131072, 0);
    for (u32 n = 0; n < 0x10000; n++) {
        u16 value = 0x30FB;  // 无效组合的中点（th15 prepare-fonts.mjs 同约定）
        if (n < 0x100) {
            const u8 byte = u8(n);
            if (byte < 0x80)
                value = byte;  // ASCII 直通
            else if (byte >= 0xA1 && byte <= 0xDF)
                value = u16(0xFF61u + (byte - 0xA1));  // 半角片假名
        } else {
            const u8 lead = u8(n >> 8), trail = u8(n & 0xFF);
            if (lead >= 0x81 && lead <= 0xEF) {
                const u8 row = kCp932Row[lead - 0x81];
                // trail 为 u8：上界 0x40+192=0x100 恒真（kCp932TrailCols=192
                // 覆盖到 0xFF），只判下界即可，与 cp932_to_utf8 同格语义。
                if (row != 0xFF && trail >= kCp932TrailBase) {
                    const u16 mapped = kCp932ToUnicode[row][trail - kCp932TrailBase];
                    if (mapped != 0) value = mapped;
                }
            }
        }
        out[n * 2] = u8(value & 0xFF);
        out[n * 2 + 1] = u8(value >> 8);
    }
    return out.size() == 131072;
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
    wait_timeout_ = 0;
    frame_pause_ = false;
    return true;
}

void MsgPlayer::finish() {
    // 0x4265a0：执行器返回非 0 → 当帧销毁对话（+0x198=0），519 下一帧放行。
    state_.done = true;
    state_.active = false;
    if (host_) host_->msg_dialog_end();  // 钩子：宿主释放对话 VM（立绘/名牌/文本区）
}

bool MsgPlayer::step(const MsgAdvance& input) {
    if (!state_.active) return false;
    frame_pause_ = false;
    // 0x428ae0:125-127：Ctrl(0x200) 或 Z(0x1) 持续 → 置快进请求位 0x40。
    // 位值经 GameKeyboard 表（0x200=Ctrl、0x80000=Enter、0x1=Z）。
    state_.skip_request = (input.held & (0x1u | 0x200u)) != 0;
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
        const Flow flow = execute(opcode, cursor_ + 4, arg_bytes, input);
        if (flow == Flow::Blocked) break;  // 指针停在阻塞指令上（如 opcode 11）
        cursor_ += 4 + arg_bytes;
        if (frame_pause_) break;  // opcode 12：本帧止步（原 +0x174=1 门）
    }
    if (state_.active) clock_++;  // 0x4265a0 rate=1 路径
    return state_.active;
}

bool MsgPlayer::step(bool z_pressed) {
    MsgAdvance input;
    input.z_pressed = z_pressed;
    input.held = z_pressed ? 1u : 0u;  // 边沿帧即持续首帧（与原 0x4e4c14 首帧一致）
    return step(input);
}

MsgPlayer::Flow MsgPlayer::execute(u8 opcode, const u8* args, u32 arg_bytes,
                                   const MsgAdvance& input) {
    switch (opcode) {
        case 0x01:  // 1：按角色装载立绘 bank（0x4c0aa0 + be7b8*4）
            host_->msg_portrait_load();
            return Flow::Advance;
        case 0x02:  // 2：右立绘创建（0x429335；宿主按 entry 分流中Boss/Boss 对）
        case 0x13:  // 19：Boss 曲播放（0x43ae40(1, stage 表+0x2c)，0x4299df）
        case 0x14:  // 20：名牌 VM（0x429a94；stage 表 +0x54../+0x5c..）
        case 0x1f:  // 31：右立绘 Boss 对（0x42948c）
            host_->msg_bgm(i32(opcode));
            return Flow::Advance;
        case 0x04:  // 4：释放左立绘槽（0x4295cf）
        case 0x05:  // 5：释放右槽（0x4295e9，含名牌 +0x5c）
        case 0x06:  // 6：释放名牌四槽 + 清文本区（0x42960c）
            host_->msg_release(i32(opcode));
            if (opcode == 0x06) state_.lines.clear();
            return Flow::Advance;
        case 0x0d:  // 13：左立绘表情切换（0x4298d4：label = arg + 0x11 立即中断）
        case 0x0e: {  // 14：右立绘表情切换（0x4298e8）
            i32 label = 0;
            if (arg_bytes >= 4) std::memcpy(&label, args, 4);
            host_->msg_portrait_interrupt(i32(opcode), label + 0x11);
            return Flow::Advance;
        }
        case 0x0c:  // 12：单帧止步（+0x5d=1）
            frame_pause_ = true;
            return Flow::Advance;
        case 0x07:  // 7：左侧说话人（0x429641：左 label2 亮、右 label3 暗）
        case 0x08:  // 8：右侧说话人（0x4296ee：左 label3 暗、右 label2 亮）
            state_.side = opcode == 0x08 ? 1 : 0;
            line_index_ = 0;
            block_open_ = 0;
            host_->msg_speaker_switch(i32(opcode));
            return Flow::Advance;
        case 0x09:  // 9：双立绘名牌变体（0x54.. 位重排）——视觉 TODO
            return Flow::Advance;
        case 0x0a: {  // 10：自动推进位翻转（arg0 & 1）
            i32 value = 0;
            if (arg_bytes >= 4) std::memcpy(&value, args, 4);
            state_.auto_advance = (value & 1) != 0;
            return Flow::Advance;
        }
        case 0x0b: {  // 11：等输入（asm 0x4298fc-0x429988 放行族）
            // 原版放行条件（0x428ae0:366-380 + 0x42991f/0x42992b/0x429930-0x429969
            // 逐指令核，r4 定稿）：
            //  a) Z/Enter 持续位（0x4e4c14 & 0x80001）→ 计声放行（0x460bc0(0)）；
            //  b) arg 倒计时（0x4016c0 置 value-1 起步、每帧 -1）≤0 → 计声放行；
            //  c) 自动位 bit0（429936 test al,0x1）+ 快进请求位 bit0x40
            //     （42993e；0x428ae0:125-127 由 Ctrl 0x200/Z 持续置位）+
            //     Ctrl（0x42b3f0(9)，索引 0x4e49f0+0x194+9*4）或 Z（(0)）
            //     持续 ≥0x14（429955/429966 cmp eax,0x14 双计数器任一）
            //     → 静默放行（42996f 无 0x460bc0 调用）。
            //  bit0/bit0x40 缺一即 429938/429940 跳回阻塞——无被动计时放行
            // （旧 port「wait_frames>=20」臆测路径已删）。
            if (wait_timeout_ == 0 && arg_bytes >= 4) {
                i32 timeout = 0;
                std::memcpy(&timeout, args, 4);
                wait_timeout_ = timeout > 0 ? timeout : -1;  // value-1 起步；无 arg = 不超时
            }
            const bool hold_z_enter = (input.held & (0x1u | 0x80000u)) != 0;
            const bool timed_out = wait_timeout_ > 0 && --wait_timeout_ <= 0;
            if (input.z_pressed || hold_z_enter || timed_out) {
                if (hold_z_enter || timed_out) host_->msg_advance_blip();
                line_index_ = 0;  // case 0xb 通过路径重置行块
                block_open_ = 0;
                state_.lines.clear();
                wait_timeout_ = 0;
                return Flow::Advance;
            }
            if (state_.auto_advance && state_.skip_request &&
                (input.ctrl_frames >= 0x14 || input.z_frames >= 0x14)) {
                line_index_ = 0;  // 静默快进：0x429946 路径不计声
                block_open_ = 0;
                state_.lines.clear();
                wait_timeout_ = 0;
                return Flow::Advance;
            }
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
        case 0x12:  // 18：文本区 + 名牌槽释放（0x429250，0x424ba0 + 四槽 0x46fc30）
        case 0x17:  // 23：释放左立绘槽（0x4295a9）
        case 0x18:  // 24：释放右立绘槽（0x4295bc）
            host_->msg_release(i32(opcode));
            return Flow::Advance;
        case 0x19:  // 25：名牌缩放四参（+0x4e4）——视觉 TODO
        case 0x1a:  // 26：显示位（0x5e |= 2）——视觉 TODO
        case 0x1e:  // 30：原版 switch 无 case（dump 18 处用例，空转）
            return Flow::Advance;
        case 0x15:  // 21：★关终了（case 0x15 → 0x42cd40，0x428ae0:673-675）
            host_->msg_stage_clear();
            return Flow::Advance;
        case 0x16:  // 22：Boss 战设置（0x43af70 音乐淡出；常数 2.0s，关6 8.0s）
        case 0x1b:  // 27：同 0x43af70（淡出时长取指令 arg 浮点）
            host_->msg_music_fade(i32(opcode));
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
