#include "Gui.hpp"

#include <cstdlib>
#include <cstring>

#include "AnmManager.hpp"
#include "AnmRenderer.hpp"
#include "Bullet.hpp"
#include "Enemy.hpp"
#include "GameGlobals.hpp"
#include "Input.hpp"
#include "Player.hpp"
#include "StageTable.hpp"

namespace th16 {

namespace {

// DAT_00492260：op 1(0) 自机立绘脚本（pl00.anm，按机体）
constexpr i32 kPlayerPortraitScript[4] = {26, 16, 22, 35};

// 0x490eb0 速度表下标 0 = DAT_004a5788（游戏速度）；Msg 的三个计时器 speed_slot 运行时恒被清零
inline i32 ins_arg_i32(const MsgInstruction& ins, int i) {
    i32 v;
    std::memcpy(&v, ins.data + i * 4, sizeof v);
    return v;
}

}  // namespace

i32 Msg::iarg(int i) const noexcept { return ins_arg_i32(*ins(), i); }

float Msg::farg(int i) const noexcept {
    float v;
    std::memcpy(&v, ins()->data + i * 4, sizeof v);
    return v;
}

// 0x42bbe0：key 从 0x77 起，每字节异或后 key += step，step 从 7 起每次 +0x10
std::string Gui::decrypt_text(const u8* src) {
    std::string out;
    u8 key = 0x77, step = 7;
    for (;;) {
        const u8 c = u8(*src++ ^ key);
        key = u8(key + step);
        step = u8(step + 0x10);
        if (c == 0) break;
        out.push_back(char(c));
    }
    return out;
}

bool Gui::init(AnmManager& anm, AnmFile* front_anm, AnmFile* logo_anm, const StageEntry& stage, std::vector<u8> msg_bytes) {
    animations = &anm;
    front = front_anm;
    logo = logo_anm;
    entry = &stage;
    msg_data = std::move(msg_bytes);
    dialogue.reset();
    boss_name_vm = vm_110 = boss_title_vm = 0;
    flags_1ac = 0;
    timer_154 = Timer{};
    timer_154.reset();
    boss_timer_display = boss_timer_shown = -1;
    if (msg_data.size() < 4) { error = "msg file too short"; return false; }
    u32 count;
    std::memcpy(&count, msg_data.data(), 4);
    if (4 + size_t(count) * 8 > msg_data.size()) { error = "msg entry table out of range"; return false; }
    sync_world();
    return true;
}

void Gui::sync_world() noexcept {
    if (world != nullptr) world->dialogue = dialogue != nullptr;
}

AnmFile* Gui::enemy_anm(i32 index) const { return enemies != nullptr ? enemies->anm_file(index) : nullptr; }

void Gui::kill_vm(u32& id) {
    if (id != 0 && animations != nullptr) animations->kill(id);
    id = 0;
}

AnmVm* Gui::find_or_clear(u32& id) {
    AnmVm* vm = animations != nullptr ? animations->find(id) : nullptr;
    if (vm == nullptr) id = 0;
    return vm;
}

void Gui::set_offset(u32& id, const Vec3& pos) {
    if (AnmVm* vm = find_or_clear(id)) vm->entity_offset = pos;
}

void Gui::draw_text(Msg& m, u32& id, const char* text, u32 color, u32 shadow, i32 font, i32 x, i32 y) {
    AnmVm* vm = find_or_clear(id);
    m.text_requests.push_back({vm != nullptr ? vm->id : 0u, std::string(text), color, shadow, font, x, y});
}

// 0x429ff0
bool Gui::start_dialogue(i32 entry_index) {
    if (entry_index == -1 || entry_index == -3) {
        // 关卡 / Boss 曲切换 + logo ANM 标题（-1 → 关卡曲 / 脚本 2；-3 → Boss 曲 / 脚本 1）
        const i32 which = entry_index == -1 ? 1 : 0;
        const char* name = which != 0 ? entry->bgm_boss_name : entry->bgm_name;
        // 符卡练习且非回放：当前曲已是目标曲（DAT_004dc0f0 文件名 == name + ".wav"）则什么都不做
        if (globals != nullptr && globals->mode() == 2 && !replay_playback && current_bgm == std::string(name) + ".wav")
            return true;
        // 0x45ed00(2, which)：切曲；成绩文件 +0x19fa6 + 曲目序号 的音乐室解锁位在 ScoreFile 接入时补
        bgm_events.push_back({2, which != 0 ? entry->bgm_boss_track : entry->bgm_track});
        current_bgm = std::string(name) + ".wav";
        if (logo != nullptr && animations != nullptr) animations->spawn(*logo, which + 1, -1);
        return true;
    }
    if (entry_index == -2) {
        // 符卡标志 bit7（续关中）→ 0x43f350 续关流程，否则 0x42e150 过关：Stage clear 流程接入时补
        error = "dialogue entry -2 (stage clear / continue) not ported";
        return false;
    }
    if (entry_index < 0) { error = "dialogue entry out of range"; return false; }
    u32 count;
    std::memcpy(&count, msg_data.data(), 4);
    if (u32(entry_index) >= count) { error = "dialogue entry out of range"; return false; }
    u32 offset;
    std::memcpy(&offset, msg_data.data() + 4 + size_t(entry_index) * 8, 4);
    if (offset + 4 > msg_data.size()) { error = "dialogue entry offset out of range"; return false; }
    if (dialogue) destroy_dialogue();
    create_dialogue(entry_index, msg_data.data() + offset);
    return true;
}

// 0x429b20
void Gui::create_dialogue(i32 entry_index, const u8* script) {
    dialogue = std::make_unique<Msg>();
    Msg& m = *dialogue;
    m.entry = entry_index;
    m.age.reset();
    m.timer.reset();
    m.wait.reset();
    m.pc = script;
    AnmFile* text = animations->file(0);  // DAT_004c17f8 text.anm
    auto make_text_vm = [&](int script_index, bool mirrored) -> u32 {
        if (text == nullptr) return 0;
        const u32 id = animations->spawn(*text, script_index, -1);
        if (mirrored) animations->interrupt_now(id, 7);
        return id;
    };
    // 两行 text.anm 0，再两行 text.anm 1；第二个各自中断 7（右侧 / 阴影版）
    m.text_vms[0] = make_text_vm(0, false);
    m.text_vms[1] = make_text_vm(0, true);
    for (int i = 0; i < 2; ++i)
        if (AnmVm* vm = find_or_clear(m.text_vms[i])) { vm->unknown_528[4] = 0x15; vm->unknown_528[5] = 0x15; }
    m.text_vms[2] = make_text_vm(1, false);
    m.text_vms[3] = make_text_vm(1, true);
    for (int i = 2; i < 4; ++i)
        if (AnmVm* vm = find_or_clear(m.text_vms[i])) { vm->unknown_528[4] = 0x15; vm->unknown_528[5] = 0x15; }
    for (u32& id : m.text_vms)
        if (AnmVm* vm = find_or_clear(id)) vm->flags_hi |= 0x1000u;
    for (u32& id : m.text_vms)
        if (AnmVm* vm = find_or_clear(id)) vm->draw_callback = 5;
    // 0x416f40 全屏消弹（不转道具）、激光全灭（LaserManager 接入时补）、0x41d900 敌机全灭
    if (bullets != nullptr) bullets->cancel_all(0);
    if (enemies != nullptr) enemies->kill_all(0, nullptr);
    m.flags &= ~Msg::kShotPressed;
    m.text_pos = Vec3{384.0f, 640.0f, 0.0f};
    m.text_width = 320.0f;
    sync_world();
}

// 0x4264a0：删立绘 / 文字 / 名牌 / 文字框 VM（0x46f1c0 口径：0x4000000 保护位、递归子树）
void Gui::destroy_dialogue() {
    if (!dialogue) return;
    Msg& m = *dialogue;
    kill_vm(m.portrait_vm);
    for (u32& id : m.face_vms) kill_vm(id);
    kill_vm(m.frame_vm);
    for (u32& id : m.text_vms) kill_vm(id);
    kill_vm(m.name_vm);
    kill_vm(m.box_vm);
    dialogue.reset();
    sync_world();
}

void Gui::shutdown() {
    destroy_dialogue();
    boss_name_vm = vm_110 = boss_title_vm = 0;
}

// 0x42c240：场景请求 8 与 DAT_004a5bec bit6 时不建
void Gui::show_boss_title() {
    if (logo == nullptr || animations == nullptr) return;
    if (globals != nullptr && (globals->scene_request == 8 || (globals->flags & 0x40u) != 0)) return;
    boss_title_vm = animations->spawn(*logo, 0, -1);
}

// 0x42c480
void Gui::ensure_boss_name() {
    if (animations == nullptr || entry == nullptr || front == nullptr) return;
    if (animations->find(boss_name_vm) != nullptr) return;
    boss_name_vm = 0;
    const i32 spell_id = globals != nullptr ? globals->spell_id : 0;
    const i32 index = spell_id < 0x29 ? entry->boss_anm[1].index : entry->boss_anm[0].index;
    if (index >= 0 && index + 0xa4 >= 0) boss_name_vm = animations->spawn(*front, index + 0xa4, -1);
}

// 0x42ba30：front.anm 0xe4 + index 文字框（entity_offset = 文字位置），子 VM 0xb4 + index / 0xd4 + index 的 $F0 = 宽
void Gui::create_text_box(Msg& m, i32 index) {
    kill_vm(m.box_vm);
    if (front == nullptr) return;
    m.box_vm = animations->spawn_effect(*front, index + 0xe4, -1, Vec3{m.text_pos.x, m.text_pos.y, 0.0f});
    AnmVm* box = find_or_clear(m.box_vm);
    if (AnmVm* child = box != nullptr ? AnmManager::find_child(*box, index + 0xb4, 0) : nullptr) child->float_vars[0] = m.text_width;
    box = find_or_clear(m.box_vm);
    if (AnmVm* child = box != nullptr ? AnmManager::find_child(*box, index + 0xd4, 0) : nullptr) child->float_vars[0] = m.text_width;
    m.box_index = index;
}

// 0x42bb30：同上两个子 VM 的 $F0 = 宽 + 16
void Gui::resize_text_box(Msg& m, i32 index) {
    const float w = m.text_width + 16.0f;
    AnmVm* box = find_or_clear(m.box_vm);
    if (AnmVm* child = box != nullptr ? AnmManager::find_child(*box, index + 0xb4, 0) : nullptr) child->float_vars[0] = w;
    box = find_or_clear(m.box_vm);
    if (AnmVm* child = box != nullptr ? AnmManager::find_child(*box, index + 0xd4, 0) : nullptr) child->float_vars[0] = w;
}

// 0x42a1d0 case 7 / 8 / 9 / 0x20 公共尾：四个文字 VM pos2.y = 0、清 kTextShadow、行计数清零
void Gui::clear_text_vms(Msg& m) {
    for (u32& id : m.text_vms)
        if (AnmVm* vm = find_or_clear(id)) vm->pos2.y = 0.0f;
    m.flags &= ~Msg::kTextShadow;
    m.lines_written = 0;
    m.lines_cleared = 0;
}

// 0x427cf0（Msg 部分）
void Gui::update(float game_speed) {
    if (!dialogue) return;
    if (!run_dialogue(game_speed)) {
        Msg& m = *dialogue;
        m.age.tick(game_speed);
        return;
    }
    destroy_dialogue();
}

// 0x42a1d0
bool Gui::run_dialogue(float game_speed) {
    Msg& m = *dialogue;
    if (m.ecl_release > 0) m.ecl_release = m.ecl_release - 1;
    const u32 held = input != nullptr ? input->held : 0u;
    const u32 pressed = input != nullptr ? input->pressed : 0u;
    if ((pressed & 0x201u) != 0) m.flags |= Msg::kShotPressed;
    // 长按射击 / 跳过键 ≥ 20 帧（DAT_004a5244 / DAT_004a5268 = 游戏输入累计按住帧数，不随连发 −8）且本段可快进：脚本时刻直接跳到下一条指令
    const bool skipping = (m.flags & (Msg::kSkippable | Msg::kShotPressed)) == (Msg::kSkippable | Msg::kShotPressed) &&
                          (((held & InputState::kSkip) != 0 && input->total_frames[9] > 0x13) ||
                           ((held & InputState::kShot) != 0 && input->total_frames[0] > 0x13));
    if (skipping) m.timer.set(i32(m.ins()->time));
    bool waiting = false;
    if (i32(m.ins()->time) <= m.timer.current) {
        do {
            const MsgInstruction& ins = *m.ins();
            if (ins.opcode == 0) return true;
            if (ins.opcode == 11) {
                // 等待按键 / 倒计时：未到时本帧结束且脚本时刻不推进
                if (m.wait.current < 1) m.wait.set(ins_arg_i32(ins, 0));
                m.wait.untick(game_speed);
                if ((pressed & InputState::kConfirmMask) == 0 && m.wait.current > 0) {
                    const bool can_skip = (m.flags & (Msg::kSkippable | Msg::kShotPressed)) == (Msg::kSkippable | Msg::kShotPressed) &&
                                          input != nullptr &&
                                          (((held & InputState::kSkip) != 0 && input->total_frames[9] >= 0x14) ||
                                           ((held & InputState::kShot) != 0 && input->total_frames[0] >= 0x14));
                    if (!can_skip) { waiting = true; break; }
                    m.wait.set(0);
                } else {
                    sfx_queue.push_back(0);
                    m.wait.set(0);
                }
                m.lines_written = 0;
                m.lines_cleared = 0;
            } else {
                execute(m, ins);
            }
            m.pc = m.pc + m.ins()->size + 4;
        } while (i32(m.ins()->time) <= m.timer.current);
    }
    if (!waiting) m.timer.tick(game_speed);

    // 文字 VM 跟随文字框子 VM（0xb4 + 下标）的屏幕位置：左侧 -36；右侧按子 VM 的 scale.x 推开
    AnmVm* box = find_or_clear(m.box_vm);
    if (box == nullptr) return false;
    AnmVm* child = AnmManager::find_child(*box, m.box_index + 0xb4, 0);
    if (child == nullptr || geometry == nullptr) return false;
    Vec3 p = geometry->world_position(*child);
    const float inv = 2.0f / geometry->window_scale;
    p.x = inv * p.x;
    if (m.side < 1) p.x = p.x - 36.0f;
    else if (1.0f <= child->scale.x) p.x = p.x + 26.0f;
    else p.x = ((child->scale.x + 0.125f) * 32.0f - 6.0f) + p.x;
    p.y = p.y * inv;
    set_offset(m.text_vms[0], p);
    set_offset(m.text_vms[2], p);
    set_offset(m.text_vms[1], p);
    set_offset(m.text_vms[3], p);
    return false;
}

void Gui::execute(Msg& m, const MsgInstruction& ins) {
    AnmManager& anm = *animations;
    const i32 a0 = ins.size >= 4 ? ins_arg_i32(ins, 0) : 0;
    const i32 a1 = ins.size >= 8 ? ins_arg_i32(ins, 1) : 0;
    const i32 font = i32((m.flags >> 1) & 1u);
    switch (ins.opcode) {
    case 1:  // 自机立绘（arg 0）或 ECL ANIM 表 5 的脚本 0xb
        if (a0 == 0) {
            if (player != nullptr && player->anm != nullptr && globals != nullptr)
                m.portrait_vm = anm.spawn(*player->anm, kPlayerPortraitScript[globals->shot & 3], -1);
        } else if (AnmFile* f = enemy_anm(5)) {
            m.portrait_vm = anm.spawn(*f, 0xb, -1);
        }
        break;
    case 2: {  // Boss 立绘：关卡表 Boss 块 a0
        const StageBossAnm& b = entry->boss_anm[a0 & 3];
        if (AnmFile* f = enemy_anm(b.portrait_slot)) m.face_vms[a0 & 3] = anm.spawn(*f, b.portrait_script, -1);
        m.unknown_1c0 = 0;
        break;
    }
    case 4:
        anm.interrupt(m.portrait_vm, 1);
        m.portrait_vm = 0;
        break;
    case 5:
        anm.interrupt(m.face_vms[a0 & 3], 1);
        m.face_vms[a0 & 3] = 0;
        anm.interrupt(m.name_vm, 1);
        break;
    case 6:
        for (u32 id : m.text_vms) anm.interrupt(id, 1);
        kill_vm(m.box_vm);
        break;
    case 7:  // 自机说话：Boss 立绘暗、自机立绘亮
        for (u32 id : m.face_vms) anm.interrupt_now(id, 3);
        anm.interrupt_now(m.portrait_vm, 2);
        anm.interrupt(m.frame_vm, 2);
        m.side = 0;
        clear_text_vms(m);
        break;
    case 8:  // Boss a0 说话
        anm.interrupt_now(m.portrait_vm, 3);
        for (int i = 0; i < 4; ++i) anm.interrupt_now(m.face_vms[i], i == a0 ? 2 : 3);
        anm.interrupt(m.frame_vm, 3);
        m.side = 1;
        clear_text_vms(m);
        break;
    case 9:  // 旁白：全部立绘暗，文字 VM 回到基点
        anm.interrupt_now(m.portrait_vm, 3);
        for (u32 id : m.face_vms) anm.interrupt_now(id, 3);
        anm.interrupt(m.frame_vm, 3);
        m.side = 0;
        set_offset(m.text_vms[0], m.anchors[0]);
        set_offset(m.text_vms[1], m.anchors[m.side]);
        if (AnmVm* vm = find_or_clear(m.text_vms[0])) vm->pos2.y = 0.0f;
        if (AnmVm* vm = find_or_clear(m.text_vms[1])) vm->pos2.y = 0.0f;
        set_offset(m.text_vms[2], m.anchors[m.side]);
        set_offset(m.text_vms[3], m.anchors[m.side]);
        clear_text_vms(m);
        break;
    case 10:
        m.flags = (m.flags & ~Msg::kSkippable) | (u32(ins.data[0]) & 1u);
        break;
    case 12:
        m.ecl_release = 1;
        break;
    case 13:
        anm.interrupt_now(m.portrait_vm, a0 + 0x11);
        break;
    case 14:
        anm.interrupt_now(m.face_vms[a1 & 3], a0 + 0x11);
        break;
    case 15: {
        const std::string text = decrypt_text(ins.data);
        draw_text(m, m.text_vms[0], text.c_str(), m.colors[m.side & 3], 0, font + 4, 0, 0);
        anm.interrupt(m.text_vms[0], 2);
        break;
    }
    case 16: {
        const std::string text = decrypt_text(ins.data);
        draw_text(m, m.text_vms[1], text.c_str(), m.colors[m.side & 3], 0, font + 4, 0, 0);
        anm.interrupt(m.text_vms[1], 2);
        break;
    }
    case 17: {  // 一行对白：首行先清空四个文字 VM，再画入 text_vms[0]；次行画入 text_vms[1]
        const std::string text = decrypt_text(ins.data);
        const i32 style = i32((m.flags & Msg::kStyleMask) >> Msg::kStyleShift);
        auto place = [&](u32& primary) {
            if (m.side < 1) {
                set_offset(primary, m.text_pos);
            } else {
                set_offset(m.text_vms[0], m.text_pos);
                set_offset(m.text_vms[2], m.text_pos);
                set_offset(m.text_vms[1], m.text_pos);
                set_offset(m.text_vms[3], m.text_pos);
            }
        };
        auto measure = [&]() {
            const i32 w = i32((u32(text.size()) * 8u) & 0xfffffff0u) - 0x1c;
            // 原版按无符号转 double（DAT_00494850 修正项）：w < 0（不足 4 字节的短句）时得到 2^32 级巨宽
            float f = float(double(u32(w)));
            f = f + f;
            if (f <= m.text_width) f = m.text_width;
            m.text_width = f;
        };
        auto positioned = [&](u32& target) {
            // "|x,y,text"：固定位置的灰阴影小字
            const char* p = text.c_str() + 1;
            const i32 x = std::atoi(p);
            const char* comma = std::strchr(p, ',');
            const i32 y = comma != nullptr ? std::atoi(comma + 1) : 0;
            const char* comma2 = comma != nullptr ? std::strchr(comma + 1, ',') : nullptr;
            const char* body = comma2 != nullptr ? comma2 + 1 : "";
            if (AnmVm* vm = find_or_clear(target)) vm->flags_hi |= 0x1000u;
            draw_text(m, target, body, 0, 0xa0a0a0u, 2, x, y);
            set_offset(target, m.text_pos);
        };
        if (m.lines_written == 0) {
            if (m.lines_cleared == 0) {
                m.text_width = 0.0f;
                draw_text(m, m.text_vms[0], "  ", m.colors[m.side & 3], 0, font, 0, 0);
                draw_text(m, m.text_vms[1], "  ", m.colors[m.side & 3], 0, font, 0, 0);
                draw_text(m, m.text_vms[2], "  ", m.colors[m.side & 3], 0, font, 0, 1);
                draw_text(m, m.text_vms[3], "  ", m.colors[m.side & 3], 0, font, 0, 1);
                m.lines_cleared = 1;
                for (u32 id : m.text_vms) anm.interrupt(id, 3);
            }
            if (!text.empty() && text[0] == '|') {
                positioned(m.text_vms[2]);
                anm.interrupt_now(m.text_vms[2], 2);
            } else {
                measure();
                create_text_box(m, m.side + style * 2);
                resize_text_box(m, m.side + style * 2);
                draw_text(m, m.text_vms[0], text.c_str(), m.colors[m.side & 3], 0, font, 0, 0);
                place(m.text_vms[0]);
                anm.interrupt_now(m.text_vms[0], 2);
                m.lines_written = m.lines_written + 1;
            }
        } else if (!text.empty() && text[0] == '|') {
            positioned(m.text_vms[3]);
            anm.interrupt(m.text_vms[3], 2);
        } else {
            measure();
            create_text_box(m, m.side + style * 2 + 8);
            resize_text_box(m, m.side + style * 2 + 8);
            draw_text(m, m.text_vms[1], text.c_str(), m.colors[m.side & 3], 0, font, 0, 0);
            place(m.text_vms[1]);
            anm.interrupt_now(m.text_vms[1], 2);
            m.lines_written = 0;
            m.lines_cleared = 0;
        }
        break;
    }
    case 18:
        kill_vm(m.box_vm);
        for (u32 id : m.text_vms) anm.interrupt(id, 3);
        break;
    case 19:  // Boss 曲 + logo ANM 脚本 2（Boss 名标题）
        bgm_events.push_back({1, entry->bgm_boss_track});
        if (logo != nullptr) anm.spawn(*logo, 2, -1);
        break;
    case 20: {  // Boss 名牌（关卡表 Boss 块 a0 的名牌 ANM）
        const StageBossAnm& b = entry->boss_anm[a0 & 3];
        if (AnmFile* f = enemy_anm(b.name_slot)) m.name_vm = anm.spawn(*f, b.name_script, -1);
        ensure_boss_name();
        break;
    }
    case 21:  // 0x42e150 过关结算：Stage clear 流程接入时补
        error = "msg op 21 (stage clear) not ported";
        break;
    case 22:
    case 27:
        bgm_events.push_back({0, -1});  // 0x43c470 淡出
        break;
    case 23:
        anm.interrupt(m.portrait_vm, 7);
        break;
    case 24:
        anm.interrupt(m.face_vms[0], 7);
        anm.interrupt(m.face_vms[1], 7);
        break;
    case 25:
        for (u32& id : m.text_vms)
            if (AnmVm* vm = find_or_clear(id)) vm->pos2.y = float(a0);
        break;
    case 26:
        m.flags |= Msg::kTextShadow;
        break;
    case 28:
        m.text_pos.x = m.farg(0) + m.farg(0);
        m.text_pos.y = m.farg(1) + m.farg(1);
        break;
    case 29:
        m.flags = (m.flags & ~Msg::kStyleMask) | ((u32(a0) << Msg::kStyleShift) & Msg::kStyleMask);
        break;
    case 31: {  // 关卡表块 1 立绘 → face_vms[1]
        const StageBossAnm& b = entry->boss_anm[1];
        if (AnmFile* f = enemy_anm(b.portrait_slot)) m.face_vms[1] = anm.spawn(*f, b.portrait_script, -1);
        m.unknown_1c0 = 0;
        break;
    }
    case 32:
        anm.interrupt(m.frame_vm, 3);
        m.side = a0;
        clear_text_vms(m);
        break;
    case 33:
        anm.interrupt_now(a0 == 0 ? m.portrait_vm : m.face_vms[a1 & 3], 3);
        break;
    case 34:
        anm.interrupt_now(a0 == 0 ? m.portrait_vm : m.face_vms[a1 & 3], 2);
        break;
    case 35:  // 0x426780：Gui+0x110 = front.anm 0x3c
        if (front != nullptr) vm_110 = anm.spawn(*front, 0x3c, -1);
        break;
    default:
        break;  // 3 / 30 及未知码：原版 switch 无分支
    }
}

}  // namespace th16
