// stage1 全局变量供给（0x420380 int 视图 / 0x420d00 float 视图的行为子集）。
// 语义依据与【待校准】清单见 docs/plans/event-contract.md；事件字段是 oracle 合同。
#include "StageSim.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace th13 {

// ---------------- 全局变量（0x420380 int 视图 / 0x420d00 float 视图） ----------------

bool StageGvars::integer(i32 id, i32& value) {
    StageSim& sim = *this->sim;
    StageEnemy* e = sim.current;
    switch (id) {
    case -10000:  // RAND（0x472c90 & 0x7fffffff）
        value = i32(sim.random.next() & 0x7fffffffu);
        return true;
    case -9999:  // RANDF 的 int 视图：ftol 截断（消耗一次抽取）
        value = i32(sim.random.unit());
        return true;
    case -9988:  // TIME int 视图（+0x14A8）
        value = i32(e ? e->age : 0);
        return true;
    case -9987:  // RANDF2 int 视图：0x472e10 → ftol
        value = i32(sim.random.signed_unit());
        return true;
    case -9986:
    case -9998:  // int getter 无对应 case，走 default 0（不消耗 RNG）
        value = 0;
        return true;
    case -9985:
    case -9984:
    case -9983:
    case -9982:
        value = e ? e->i_slots[id + 9985] : 0;
        return true;
    case -9981:
    case -9980:
    case -9979:
    case -9978:  // F0-F3 int 视图（ftol）
        value = e ? i32(e->f_slots[id + 9981]) : 0;
        return true;
    case -9963: {
        StageEnemy* boss = sim.boss();
        value = boss ? i32(boss->position[0]) : 0;
        return true;
    }
    case -9962: {
        StageEnemy* boss = sim.boss();
        value = boss ? i32(boss->position[1]) : 0;
        return true;
    }
    case -9960:  // RANK
        value = 3;
        return true;
    case -9959:  // DIFF
        value = 3;
        return true;
    case -9957:
        value = 1;
        return true;
    case -9946:
        // ENM_CNT_REAL（mgr+0xB8）：0x420638 直读计数器，计数在链表摘除时
        // 才减——遍历中途读取时，已标 dead 但未被本趟游标到达的敌机仍在列。
        // 移植侧 vector 摘除同拍（step_frame 游标到达时擦除），故计数 =
        // 当前 vector 大小（含 root；勿用 !dead 过滤——会把已标未摘者漏计，
        // 回放实测 frame 3252 由此少 1 导致 Phantom01 生成门 [-9946]<11 误开）。
        value = i32(sim.enemies.size());
        return true;
    case -9909:  // 0x420380 case 0xffffd94b：+0x11E8（trace_id，0x4183f0:61 的
        // param2[0x14]）。oracle 实测 Phantom01（MBoss1_at 子线程创建）求值
        // 得 75 = MBoss id（433 的 moveEnm 目标）。
        value = e ? e->trace_id : 0;
        return true;
    case -9908:  // ENM_CNT（FUN_00417e90，未定域）
        value = 0;
        return true;
    case -9907:  // SPELL_ID
        value = -1;
        return true;
    case -9914:  // ID（+0x11E4）
        value = e ? e->script_id : 0;
        return true;
    default:
        for (const auto& pair : writable_ints_)
            if (pair.first == id) {
                value = pair.second;
                return true;
            }
        value = 0;
        return true;
    }
}

bool StageGvars::floating(i32 id, float& value) {
    StageSim& sim = *this->sim;
    StageEnemy* e = sim.current;
    switch (id) {
    case -10000:
        value = float(sim.random.next() & 0x7fffffffu);
        return true;
    case -9999:  // RANDF（0x472de0）
        value = sim.random.unit();
        return true;
    case -9998:  // RANDRAD（0x472e10 × π）
        value = float(sim.random.signed_unit() * 3.1415927410125732);
        return true;
    case -9997:  // FINAL_X/Y（主位置 +0x1230）
    case -9996:
        value = e ? e->position[id == -9997 ? 0 : 1] : 0.0f;
        return true;
    case -9995:  // ABS_X/Y（+0x1274，ctor 写出生点后未见解更新——窗口内无人读）
    case -9994:
        value = e ? e->vel.pos[id == -9995 ? 0 : 1] : 0.0f;
        return true;
    case -9993:  // REL_X/Y（+0x12B8，恒 0）
    case -9992:
        value = 0.0f;
        return true;
    case -9991:
        value = sim.player_x;
        return true;
    case -9990:
        value = sim.player_y;
        return true;
    case -9989: {  // ANGLE_PLAYER（0x443f60：自机位置指向玩家）
        const float dy = sim.player_y - (e ? e->position[1] : 0.0f);
        const float dx = sim.player_x - (e ? e->position[0] : 0.0f);
        if (dy == 0.0f && dx == 0.0f) {
            value = 1.5707964f;
        } else {
            value = float(std::atan2(static_cast<double>(dy), static_cast<double>(dx)));
        }
        return true;
    }
    case -9988:
        value = e ? e->age : 0.0f;
        return true;
    case -9987:
        value = sim.random.signed_unit();
        return true;
    case -9986:
        value = 0.0f;
        return true;
    case -9985:
    case -9984:
    case -9983:
    case -9982:
        value = float(e ? e->i_slots[id + 9985] : 0);
        return true;
    case -9981:
    case -9980:
    case -9979:
    case -9978:
        value = e ? e->f_slots[id + 9981] : 0.0f;
        return true;
    case -9963: {
        StageEnemy* boss = sim.boss();
        value = boss ? boss->position[0] : 0.0f;
        return true;
    }
    case -9962: {
        StageEnemy* boss = sim.boss();
        value = boss ? boss->position[1] : 128.0f;
        return true;
    }
    case -9960:
    case -9959:
        value = 3.0f;
        return true;
    case -9957:
        value = 1.0f;
        return true;
    default:
        value = 0.0f;
        return true;
    }
}

i32* StageGvars::integer_destination(i32 id) {
    StageEnemy* e = sim->current;
    if (e && id >= -9985 && id <= -9982) return &e->i_slots[id + 9985];
    if (id == -9914 && e) return &e->script_id;
    for (auto& pair : writable_ints_)
        if (pair.first == id) return &pair.second;
    if (id == -9949 || id == -9948 || id == -9947) {  // MISS/BOMB/CAPTURE 计数（MBoss 族写入）
        writable_ints_.emplace_back(id, 0);
        return &writable_ints_.back().second;
    }
    return nullptr;
}

float* StageGvars::float_destination(i32 id) {
    StageEnemy* e = sim->current;
    if (e && id >= -9981 && id <= -9978) return &e->f_slots[id + 9981];
    return nullptr;
}

}  // namespace th13
