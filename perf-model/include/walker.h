#pragma once

#include <cstdint>
#include <deque>
#include <vector>

#include "config.h"
#include "dtlb.h"
#include "memory.h"
#include "txn.h"

namespace perf {

// Multi-level hardware page-table walker pool with MSHR queue and optional miss coalescing.
class PageTableWalker {
 public:
  explicit PageTableWalker(const WalkerConfig& cfg);

  // Advance active walks using memory completions ready at `cycle`.
  void tick(uint64_t cycle, MemoryModel& mem, Dtlb& dtlb, std::vector<Txn*>& completed_out);

  // Accept a DTLB miss resolved at `cycle` (coalesce, launch walk, or enqueue in MSHR).
  void on_miss(Txn* txn, uint64_t cycle, MemoryModel& mem);

  // Record per-cycle walker occupancy after all stage handoffs for `cycle` have occurred.
  void sample_cycle();

  uint32_t free_walkers() const;
  uint32_t busy_walkers() const { return cfg_.count - free_walkers(); }
  uint32_t total_walkers() const { return cfg_.count; }
  uint32_t active_walks() const {
    return busy_walkers() + static_cast<uint32_t>(mshr_queue_.size());
  }
  // Total miss-handling capacity available (idle walkers + free MSHR queue slots).
  uint32_t mshr_capacity_free() const;
  bool empty() const { return busy_walkers() == 0 && mshr_queue_.empty(); }

  uint64_t walks_started() const { return walks_started_; }
  uint64_t walks_completed() const { return walks_completed_; }
  uint64_t coalesced_misses() const { return coalesced_misses_; }
  uint64_t busy_walker_cycles() const { return busy_walker_cycles_; }

 private:
  struct WalkerSlot {
    bool active = false;
    uint64_t vpn = 0;
    uint64_t vaddr = 0;
    uint32_t current_level = 0;
    std::vector<Txn*> txns;
  };

  struct PendingMiss {
    uint64_t vpn = 0;
    uint64_t vaddr = 0;
    std::vector<Txn*> txns;
  };

  static uint64_t make_pte_paddr(uint64_t vpn, uint32_t level);
  int find_free_walker() const;
  void launch_on_walker(uint32_t idx, uint64_t vpn, uint64_t vaddr, std::vector<Txn*> txns,
                        uint64_t cycle, MemoryModel& mem);
  void drain_mshr_to_walkers(uint64_t cycle, MemoryModel& mem);

  WalkerConfig cfg_;
  std::vector<WalkerSlot> slots_;
  std::deque<PendingMiss> mshr_queue_;

  uint64_t walks_started_ = 0;
  uint64_t walks_completed_ = 0;
  uint64_t coalesced_misses_ = 0;
  uint64_t busy_walker_cycles_ = 0;
};

}  // namespace perf
