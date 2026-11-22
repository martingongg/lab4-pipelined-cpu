#include "dtlb.h"

#include <limits>

namespace perf {

namespace {

uint32_t log2_pow2(uint64_t v) {
  uint32_t shift = 0;
  while ((v >> shift) > 1) {
    ++shift;
  }
  return shift;
}

}  // namespace

Dtlb::Dtlb(const DtlbConfig& cfg, bool record_events)
    : cfg_(cfg), record_events_(record_events) {
  page_shift_ = log2_pow2(cfg_.page_bytes);
  num_sets_ = cfg_.entries / cfg_.ways;
  set_bits_ = log2_pow2(num_sets_);
  set_mask_ = static_cast<uint64_t>(num_sets_ - 1);
  sets_.assign(num_sets_, std::vector<Entry>(cfg_.ways));
}

bool Dtlb::peek_hit(uint64_t vaddr) const {
  const uint64_t vpn = vaddr_to_vpn(vaddr);
  const uint32_t sidx = set_index(vpn);
  const uint64_t tag = vpn_tag(vpn);
  for (const auto& e : sets_[sidx]) {
    if (e.valid && e.tag == tag) {
      return true;
    }
  }
  return false;
}

bool Dtlb::lookup_and_touch(uint64_t vaddr, uint64_t cycle, uint64_t txn_id) {
  const uint64_t vpn = vaddr_to_vpn(vaddr);
  const uint32_t sidx = set_index(vpn);
  const uint64_t tag = vpn_tag(vpn);
  auto& set = sets_[sidx];
  for (auto& e : set) {
    if (e.valid && e.tag == tag) {
      e.last_used_seq = ++lru_seq_;
      ++hits_;
      if (record_events_) {
        events_.push_back(TlbEvent{cycle, false, txn_id, vaddr, true});
      }
      return true;
    }
  }
  ++misses_;
  if (record_events_) {
    events_.push_back(TlbEvent{cycle, false, txn_id, vaddr, false});
  }
  return false;
}

void Dtlb::install(uint64_t vaddr, uint64_t cycle, uint64_t txn_id) {
  if (record_events_) {
    events_.push_back(TlbEvent{cycle, true, txn_id, vaddr, false});
  }
  const uint64_t vpn = vaddr_to_vpn(vaddr);
  const uint32_t sidx = set_index(vpn);
  const uint64_t tag = vpn_tag(vpn);
  auto& set = sets_[sidx];

  // If already resident (e.g., race between non-coalesced walks), refresh LRU.
  for (auto& e : set) {
    if (e.valid && e.tag == tag) {
      e.last_used_seq = ++lru_seq_;
      return;
    }
  }

  // Prefer first invalid way; otherwise evict strict LRU way.
  size_t victim = 0;
  uint64_t min_seq = std::numeric_limits<uint64_t>::max();
  for (size_t i = 0; i < set.size(); ++i) {
    if (!set[i].valid) {
      victim = i;
      break;
    }
    if (set[i].last_used_seq < min_seq) {
      min_seq = set[i].last_used_seq;
      victim = i;
    }
  }
  set[victim].valid = true;
  set[victim].tag = tag;
  set[victim].last_used_seq = ++lru_seq_;
}

void Dtlb::start_lookup(Txn* txn, uint64_t cycle) {
  txn->t_lookup = cycle;
  lookup_pipe_.push_back(PendingLookup{txn, cycle + cfg_.hit_cycles});
}

void Dtlb::tick(uint64_t cycle, std::vector<Txn*>& resolved_hits,
                std::vector<Txn*>& resolved_misses) {
  while (!lookup_pipe_.empty() && lookup_pipe_.front().resolve_cycle <= cycle) {
    Txn* txn = lookup_pipe_.front().txn;
    lookup_pipe_.pop_front();
    const bool hit = lookup_and_touch(txn->vaddr, cycle, txn->id);
    txn->tlb_hit = hit;
    if (hit) {
      txn->t_done = cycle;
      txn->completed = true;
      resolved_hits.push_back(txn);
    } else {
      resolved_misses.push_back(txn);
    }
  }
}

}  // namespace perf
