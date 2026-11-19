#include "EndingScene.hpp"

namespace th16 {

void EndingScene::begin(i32 shot, i32 shot_aux, bool has_true_end) {
    *this = EndingScene{};
    live = true;
    ending_index = (shot + shot_aux) * 2 + (has_true_end ? 1 : 0);
    events.push_back("begin");
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
