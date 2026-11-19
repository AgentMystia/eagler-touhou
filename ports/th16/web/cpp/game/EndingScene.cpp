#include "EndingScene.hpp"

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
    events.push_back("msg-loaded");
    return true;
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
