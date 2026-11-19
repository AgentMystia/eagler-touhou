#include "EndingScene.hpp"

#include "AnmManager.hpp"
#include "ScoreFile.hpp"

#include <cstring>

namespace th16 {

void EndingScene::begin(i32 shot, i32 shot_aux, i32 continues, ScoreFile* scores, i32 difficulty) {
    *this = EndingScene{};
    live = true;
    const i32 combo = shot + shot_aux;
    // 0x4191f0：continues==0 → ×2；否则 ×2+1
    ending_index = combo * 2 + (continues == 0 ? 0 : 1);
    if (ending_index < 0) ending_index = 0;
    if (ending_index > 7) ending_index = 7;

    if (scores != nullptr) {
        // +0x19f96[ending_index]：未见过 → flags bit0；+0x19f9e 全机未见 → bit1；然后置见过
        if (scores->ending_seen[ending_index] == 0) flags |= 1u;
        if (!scores->any_ending_seen) flags |= 2u;
        scores->ending_seen[ending_index] = u8(scores->ending_seen[ending_index] | 1);
        if (difficulty == 1) scores->ending_seen[ending_index] = u8(scores->ending_seen[ending_index] | 2);
        else if (difficulty == 2) scores->ending_seen[ending_index] = u8(scores->ending_seen[ending_index] | 4);
        else if (difficulty == 3) scores->ending_seen[ending_index] = u8(scores->ending_seen[ending_index] | 8);
        if (ending_index < 8) scores->any_ending_seen = 1;
    }
    msg_name = kMsgNames[ending_index];
    events.push_back("begin");
}

bool EndingScene::load_msg(const u8* bytes, u32 size, const char* name) {
    if (bytes == nullptr || size == 0) {
        events.push_back("msg-missing");
        return false;
    }
    msg_bytes.assign(bytes, bytes + size);
    if (name != nullptr) msg_name = name;
    // 原版 0x4191f0：解密后缓冲；脚本起点 = header+4 偏移表首条（与 Gui Msg 同布局时）
    if (msg_bytes.size() >= 8) {
        u32 count = 0, offset = 0;
        std::memcpy(&count, msg_bytes.data(), 4);
        std::memcpy(&offset, msg_bytes.data() + 4, 4);
        if (count > 0 && offset < msg_bytes.size()) script_pc = msg_bytes.data() + offset;
        else script_pc = msg_bytes.data();
    } else {
        script_pc = msg_bytes.data();
    }
    events.push_back("msg-loaded");
    return true;
}

void EndingScene::spawn_line_vms(AnmManager& anm, AnmFile* text_anm) {
    if (text_anm == nullptr) return;
    for (int i = 0; i < 5; ++i) {
        AnmVm* vm = nullptr;
        line_vms[i] = anm.spawn(*text_anm, 0x2e + i, -1, &vm);
        if (vm != nullptr) {
            anm_set_layer(*vm, 0x10);                 // 0x4197c0：层组 0x10
            vm->flags_lo &= ~0x1000u;                 // 清 bit12
        }
    }
    if (ending_index >= 8) flags |= 2u;
    events.push_back("vms");
}

void EndingScene::destroy_line_vms(AnmManager& anm) {
    for (u32& id : line_vms) {
        if (id != 0) anm.kill(id);
        id = 0;
    }
}

void EndingScene::end() {
    live = false;
    finished = true;
    events.push_back("end");
}

bool EndingScene::update(const InputState& input) {
    if (!live) return false;
    ++timer;
    if (timer > kSkipFrames && (input.pressed & (InputState::kConfirmMask | InputState::kCancelMask)) != 0) {
        end();
        return true;
    }
    if (timer > kAutoFrames) {
        end();
        return true;
    }
    return false;
}

}  // namespace th16
