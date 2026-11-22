#pragma once

#include <cstdint>
#include <map>
#include <unordered_set>
#include <vector>

#include "config.h"
#include "txn.h"

namespace perf {

// Reorder / Retire stage supporting strict in-order retirement by global transaction ID
// or out-of-order immediate retirement.
class RetireStage {
 public:
  explicit RetireStage(const RetireConfig& cfg);

  // Sliding ROB-window eligibility prevents head-of-ROB allocation deadlock under in-order retire.
  bool can_accept(uint64_t txn_id) const;
  uint32_t rob_free() const;

  void on_dispatch(Txn* txn);
  void on_complete(Txn* txn);
  void tick(uint64_t cycle, std::vector<Txn*>& retired_out);
  void sample_cycle();

  bool empty() const { return in_flight_.empty() && ready_.empty(); }
  uint32_t occupancy() const { return static_cast<uint32_t>(in_flight_.size()); }
  uint32_t max_occupancy() const { return max_occupancy_; }
  uint64_t occupancy_sum() const { return occupancy_sum_; }
  uint64_t retired_count() const { return retired_count_; }
  uint64_t next_retire_id() const { return next_retire_id_; }

 private:
  RetireConfig cfg_;
  uint64_t next_retire_id_ = 0;
  uint64_t retired_count_ = 0;
  uint32_t max_occupancy_ = 0;
  uint64_t occupancy_sum_ = 0;

  std::unordered_set<uint64_t> in_flight_;
  std::map<uint64_t, Txn*> ready_;
};

}  // namespace perf
