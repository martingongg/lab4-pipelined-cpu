#include "memory.h"

namespace perf {

MemoryModel::MemoryModel(const MemoryConfig& cfg) : cfg_(cfg) {}

bool MemoryModel::can_dispatch_now() const {
  if (cfg_.max_requests_per_cycle > 0 && issued_this_cycle_ >= cfg_.max_requests_per_cycle) {
    return false;
  }
  if (cfg_.max_in_flight > 0 && in_flight_.size() >= cfg_.max_in_flight) {
    return false;
  }
  return true;
}

void MemoryModel::dispatch_pending(uint64_t cycle) {
  while (!pending_queue_.empty() && can_dispatch_now()) {
    PendingReq req = pending_queue_.front();
    pending_queue_.pop_front();
    in_flight_.push(MemResponse{req.req_id, req.walker_id, req.paddr, cycle + cfg_.access_cycles});
    ++issued_this_cycle_;
  }
}

uint64_t MemoryModel::submit(uint64_t paddr, uint64_t cycle, uint32_t walker_id) {
  if (!cycle_initialized_ || cycle != last_cycle_) {
    last_cycle_ = cycle;
    cycle_initialized_ = true;
    issued_this_cycle_ = 0;
  }
  const uint64_t id = next_req_id_++;
  ++total_requests_;
  pending_queue_.push_back(PendingReq{id, walker_id, paddr});
  dispatch_pending(cycle);
  return id;
}

void MemoryModel::tick(uint64_t cycle) {
  if (!cycle_initialized_ || cycle != last_cycle_) {
    last_cycle_ = cycle;
    cycle_initialized_ = true;
    issued_this_cycle_ = 0;
  }
  while (!in_flight_.empty() && in_flight_.top().ready_cycle <= cycle) {
    completed_.push_back(in_flight_.top());
    in_flight_.pop();
  }
  dispatch_pending(cycle);
}

std::vector<MemResponse> MemoryModel::drain_completed() {
  std::vector<MemResponse> out;
  out.swap(completed_);
  return out;
}

}  // namespace perf
