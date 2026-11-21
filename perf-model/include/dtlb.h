#pragma once

#include <cstdint>
#include <deque>
#include <vector>

#include "config.h"
#include "txn.h"

namespace perf {

class Dtlb {
 public:
  explicit Dtlb(const DtlbConfig& cfg, bool record_events = false);

  uint64_t vaddr_to_vpn(uint64_t vaddr) const { return vaddr >> page_shift_; }
  uint32_t page_shift() const { return page_shift_; }
  uint32_t num_sets() const { return num_sets_; }
  uint32_t ways() const { return cfg_.ways; }

  // Non-mutating probe used to measure HoL stalls for queued requests.
  bool peek_hit(uint64_t vaddr) const;

  // Immediate tag lookup (updates LRU on hit) and fill (evicts LRU way on full set).
  bool lookup_and_touch(uint64_t vaddr, uint64_t cycle, uint64_t txn_id);
  void install(uint64_t vaddr, uint64_t cycle, uint64_t txn_id);

  // Pipelined lookup stage taking cfg_.hit_cycles cycles.
  void start_lookup(Txn* txn, uint64_t cycle);
  void tick(uint64_t cycle, std::vector<Txn*>& resolved_hits, std::vector<Txn*>& resolved_misses);

  uint32_t in_flight() const { return static_cast<uint32_t>(lookup_pipe_.size()); }
  bool empty() const { return lookup_pipe_.empty(); }

  uint64_t hits() const { return hits_; }
  uint64_t misses() const { return misses_; }
  const std::vector<TlbEvent>& events() const { return events_; }

 private:
  struct Entry {
    bool valid = false;
    uint64_t tag = 0;
    uint64_t last_used_seq = 0;
  };

  struct PendingLookup {
    Txn* txn = nullptr;
    uint64_t resolve_cycle = 0;
  };

  uint32_t set_index(uint64_t vpn) const { return static_cast<uint32_t>(vpn & set_mask_); }
  uint64_t vpn_tag(uint64_t vpn) const { return vpn >> set_bits_; }

  DtlbConfig cfg_;
  bool record_events_ = false;
  uint32_t page_shift_ = 12;
  uint32_t num_sets_ = 1;
  uint32_t set_bits_ = 0;
  uint64_t set_mask_ = 0;

  std::vector<std::vector<Entry>> sets_;
  std::deque<PendingLookup> lookup_pipe_;
  uint64_t lru_seq_ = 0;
  uint64_t hits_ = 0;
  uint64_t misses_ = 0;
  std::vector<TlbEvent> events_;
};

}  // namespace perf
