#include "Chain.hpp"

#include <algorithm>

namespace th16 {

ChainEntry* ChainManager::create(ChainCallback cb, i32 priority, bool update_list, void* context) {
    auto entry = std::make_unique<ChainEntry>();
    entry->priority = priority;
    entry->flags = kFlagOwned;  // 创建时未启用（与 0x43af60 `& ~2` 一致）
    entry->callback = std::move(cb);
    entry->context = context;
    entry->in_update = update_list;
    ChainEntry* raw = entry.get();
    if (update_list) insert_sorted(update_, std::move(entry));
    else insert_sorted(draw_, std::move(entry));
    return raw;
}

void ChainManager::enable(ChainEntry* entry) {
    if (entry) entry->flags |= kFlagEnabled;
}

void ChainManager::disable(ChainEntry* entry) {
    if (entry) entry->flags &= ~kFlagEnabled;
}

void ChainManager::remove(ChainEntry* entry) {
    if (entry == nullptr) return;
    auto erase_from = [entry](std::vector<std::unique_ptr<ChainEntry>>& list) {
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [entry](const std::unique_ptr<ChainEntry>& p) { return p.get() == entry; }),
                   list.end());
    };
    erase_from(update_);
    erase_from(draw_);
}

void ChainManager::insert_sorted(std::vector<std::unique_ptr<ChainEntry>>& list, std::unique_ptr<ChainEntry> entry) {
    // 0x401300：插入时若有 init 回调则立即调用
    if (entry->init) {
        entry->init();
        entry->init = nullptr;
    }
    const i32 prio = entry->priority;
    auto it = list.begin();
    while (it != list.end() && (*it)->priority < prio) ++it;
    list.insert(it, std::move(entry));
}

i32 ChainManager::run_list(std::vector<std::unique_ptr<ChainEntry>>& list) {
    i32 ran = 0;
    for (size_t i = 0; i < list.size();) {
        ChainEntry* e = list[i].get();
        if (e == nullptr || !e->callback || (e->flags & kFlagEnabled) == 0) {
            ++i;
            continue;
        }
        const ChainResult r = e->callback();
        switch (r) {
        case ChainResult::Remove:
            list.erase(list.begin() + i32(i));
            break;
        case ChainResult::Restart:
            i = 0;
            ran = 0;
            continue;
        case ChainResult::BreakExit:
            return 1;
        case ChainResult::BreakOk:
            return 0;
        case ChainResult::BreakNeg:
            return -1;
        case ChainResult::Continue:
        default:
            ++ran;
            ++i;
            break;
        }
    }
    return ran > 0 ? 1 : 0;
}

i32 ChainManager::run_update() { return run_list(update_); }
i32 ChainManager::run_draw() { return run_list(draw_); }

}  // namespace th16
