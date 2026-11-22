#pragma once

#include <cstdint>
#include <deque>
#include <queue>
#include <vector>

#include "config.h"

namespace perf {

struct MemResponse {
  uint64_t req_id = 0;
  uint32_t walker_id = 0;
  uint64_t paddr = 0;
  uint64_t ready_cycle = 0;
};

// Fixed-latency memory model with optional per-cycle issue bandwidth and in-flight caps.
class MemoryModel {
 public:
  explicit MemoryModel(const MemoryConfig& cfg);

  // Submit a page-table memory read at `cycle`; starts immediately if BW/queue allows.
  uint64_t submit(uint64_t paddr, uint64_t cycle, uint32_t walker_id);

  // Advance memory state to `cycle`: collect completions ready at `cycle` and
  // dispatch any queued requests whose bandwidth/concurrency slots opened.
  void tick(uint64_t cycle);

  std::vector<MemResponse> drain_completed();

  bool empty() const { return pending_queue_.empty() && in_flight_.empty(); }
  uint64_t total_requests() const { return total_requests_; }

 private:
  struct PendingReq {
    uint64_t req_id = 0;
    uint32_t walker_id = 0;
    uint64_t paddr = 0;
  };

  struct ReadyCompare {
    bool operator()(const MemResponse& a, const MemResponse& b) const {
      if (a.ready_cycle != b.ready_cycle) return a.ready_cycle > b.ready_cycle;
      return a.req_id > b.req_id;
    }
  };

  bool can_dispatch_now() const;
  void dispatch_pending(uint64_t cycle);

  MemoryConfig cfg_;
  uint64_t next_req_id_ = 1;
  uint64_t total_requests_ = 0;
  uint64_t last_cycle_ = 0;
  bool cycle_initialized_ = false;
  uint32_t issued_this_cycle_ = 0;

  std::deque<PendingReq> pending_queue_;
  std::priority_queue<MemResponse, std::vector<MemResponse>, ReadyCompare> in_flight_;
  std::vector<MemResponse> completed_;
};

}  // namespace perf
