#include "walker.h"

namespace perf {

PageTableWalker::PageTableWalker(const WalkerConfig& cfg) : cfg_(cfg), slots_(cfg.count) {}

uint64_t PageTableWalker::make_pte_paddr(uint64_t vpn, uint32_t level) {
  // Deterministic synthetic physical address for page-table level `level` (aligned to 8B PTE).
  return (static_cast<uint64_t>(level) << 40) | ((vpn & 0x0000000FFFFFFFFFULL) << 3);
}

int PageTableWalker::find_free_walker() const {
  for (size_t i = 0; i < slots_.size(); ++i) {
    if (!slots_[i].active) return static_cast<int>(i);
  }
  return -1;
}

uint32_t PageTableWalker::free_walkers() const {
  uint32_t free_cnt = 0;
  for (const auto& s : slots_) {
    if (!s.active) ++free_cnt;
  }
  return free_cnt;
}

uint32_t PageTableWalker::mshr_capacity_free() const {
  const uint32_t q_free = (cfg_.mshr_entries > mshr_queue_.size())
                              ? static_cast<uint32_t>(cfg_.mshr_entries - mshr_queue_.size())
                              : 0;
  return free_walkers() + q_free;
}

void PageTableWalker::launch_on_walker(uint32_t idx, uint64_t vpn, uint64_t vaddr,
                                       std::vector<Txn*> txns, uint64_t cycle, MemoryModel& mem) {
  auto& slot = slots_[idx];
  slot.active = true;
  slot.vpn = vpn;
  slot.vaddr = vaddr;
  slot.current_level = 1;
  slot.txns = std::move(txns);
  ++walks_started_;
  mem.submit(make_pte_paddr(vpn, 1), cycle, idx);
}

void PageTableWalker::drain_mshr_to_walkers(uint64_t cycle, MemoryModel& mem) {
  while (!mshr_queue_.empty()) {
    int idx = find_free_walker();
    if (idx < 0) break;
    PendingMiss pm = std::move(mshr_queue_.front());
    mshr_queue_.pop_front();
    launch_on_walker(static_cast<uint32_t>(idx), pm.vpn, pm.vaddr, std::move(pm.txns), cycle, mem);
  }
}

void PageTableWalker::tick(uint64_t cycle, MemoryModel& mem, Dtlb& dtlb,
                           std::vector<Txn*>& completed_out) {
  auto responses = mem.drain_completed();
  for (const auto& resp : responses) {
    auto& slot = slots_[resp.walker_id];
    if (!slot.active) continue;

    if (slot.current_level < cfg_.levels) {
      ++slot.current_level;
      mem.submit(make_pte_paddr(slot.vpn, slot.current_level), cycle, resp.walker_id);
    } else {
      const uint64_t fill_txn_id = slot.txns.empty() ? 0 : slot.txns.front()->id;
      dtlb.install(slot.vaddr, cycle, fill_txn_id);
      for (Txn* t : slot.txns) {
        t->t_done = cycle;
        t->completed = true;
        completed_out.push_back(t);
      }
      ++walks_completed_;
      slot.active = false;
      slot.vpn = 0;
      slot.vaddr = 0;
      slot.current_level = 0;
      slot.txns.clear();
    }
  }
  drain_mshr_to_walkers(cycle, mem);
}

void PageTableWalker::on_miss(Txn* txn, uint64_t cycle, MemoryModel& mem) {
  if (cfg_.coalesce_misses) {
    for (auto& slot : slots_) {
      if (slot.active && slot.vpn == txn->vpn) {
        txn->coalesced = true;
        slot.txns.push_back(txn);
        ++coalesced_misses_;
        return;
      }
    }
    for (auto& pm : mshr_queue_) {
      if (pm.vpn == txn->vpn) {
        txn->coalesced = true;
        pm.txns.push_back(txn);
        ++coalesced_misses_;
        return;
      }
    }
  }

  int free_idx = find_free_walker();
  if (free_idx >= 0) {
    launch_on_walker(static_cast<uint32_t>(free_idx), txn->vpn, txn->vaddr, {txn}, cycle, mem);
  } else {
    mshr_queue_.push_back(PendingMiss{txn->vpn, txn->vaddr, {txn}});
  }
}

void PageTableWalker::sample_cycle() {
  busy_walker_cycles_ += busy_walkers();
}

}  // namespace perf
