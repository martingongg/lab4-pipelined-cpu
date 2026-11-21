#include "retire.h"

#include <algorithm>

namespace perf {

RetireStage::RetireStage(const RetireConfig& cfg) : cfg_(cfg) {}

bool RetireStage::can_accept(uint64_t txn_id) const {
  if (in_flight_.size() >= cfg_.rob_entries) {
    return false;
  }
  if (cfg_.in_order) {
    return txn_id < next_retire_id_ + cfg_.rob_entries;
  }
  return true;
}

uint32_t RetireStage::rob_free() const {
  return (cfg_.rob_entries > in_flight_.size())
             ? static_cast<uint32_t>(cfg_.rob_entries - in_flight_.size())
             : 0;
}

void RetireStage::on_dispatch(Txn* txn) {
  in_flight_.insert(txn->id);
}

void RetireStage::on_complete(Txn* txn) {
  ready_[txn->id] = txn;
}

void RetireStage::tick(uint64_t cycle, std::vector<Txn*>& retired_out) {
  uint32_t retired_this_cycle = 0;
  const uint32_t limit = (cfg_.retire_width == 0) ? UINT32_MAX : cfg_.retire_width;

  if (cfg_.in_order) {
    while (retired_this_cycle < limit && !ready_.empty()) {
      auto it = ready_.begin();
      if (it->first != next_retire_id_) {
        break;
      }
      Txn* txn = it->second;
      ready_.erase(it);
      in_flight_.erase(txn->id);
      txn->t_retire = cycle;
      txn->retired = true;
      retired_out.push_back(txn);
      ++next_retire_id_;
      ++retired_count_;
      ++retired_this_cycle;
    }
  } else {
    while (retired_this_cycle < limit && !ready_.empty()) {
      auto it = ready_.begin();
      Txn* txn = it->second;
      ready_.erase(it);
      in_flight_.erase(txn->id);
      txn->t_retire = cycle;
      txn->retired = true;
      retired_out.push_back(txn);
      ++retired_count_;
      ++retired_this_cycle;
    }
  }
}

void RetireStage::sample_cycle() {
  const uint32_t occ = occupancy();
  max_occupancy_ = std::max(max_occupancy_, occ);
  occupancy_sum_ += occ;
}

}  // namespace perf
