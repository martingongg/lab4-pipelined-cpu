#include "arbiter.h"

#include <limits>
#include <stdexcept>

namespace perf {

Txn* InOrderFifoPolicy::pick(uint64_t /*cycle*/, PipelineState& state,
                             const std::function<bool(uint64_t)>& rob_accepts) {
  if (state.lookup_port_busy || state.rob_free == 0) {
    return nullptr;
  }
  // Conservative walker reservation: guarantee a walker is free if this lookup misses.
  // With walker.count == 1, the head must finish its lookup (and walk, on miss) before next starts.
  if (state.free_walkers <= state.lookup_in_flight) {
    return nullptr;
  }

  int best_src = -1;
  uint64_t min_id = std::numeric_limits<uint64_t>::max();
  for (size_t i = 0; i < state.requester_queues.size(); ++i) {
    if (!state.requester_queues[i].empty()) {
      Txn* front = state.requester_queues[i].front();
      if (front->id < min_id) {
        min_id = front->id;
        best_src = static_cast<int>(i);
      }
    }
  }
  if (best_src < 0) return nullptr;

  Txn* candidate = state.requester_queues[static_cast<size_t>(best_src)].front();
  if (!rob_accepts(candidate->id)) {
    return nullptr;
  }
  state.requester_queues[static_cast<size_t>(best_src)].pop_front();
  return candidate;
}

Txn* RoundRobinPolicy::pick(uint64_t /*cycle*/, PipelineState& state,
                            const std::function<bool(uint64_t)>& rob_accepts) {
  if (state.lookup_port_busy || state.rob_free == 0) {
    return nullptr;
  }
  if (state.free_walkers <= state.lookup_in_flight) {
    return nullptr;
  }

  const size_t n = state.requester_queues.size();
  if (n == 0) return nullptr;

  for (size_t offset = 0; offset < n; ++offset) {
    const size_t src = (static_cast<size_t>(next_src_) + offset) % n;
    auto& q = state.requester_queues[src];
    if (!q.empty() && rob_accepts(q.front()->id)) {
      Txn* chosen = q.front();
      q.pop_front();
      next_src_ = static_cast<uint32_t>((src + 1) % n);
      return chosen;
    }
  }
  return nullptr;
}

Txn* IdAwareBypassPolicy::pick(uint64_t /*cycle*/, PipelineState& state,
                               const std::function<bool(uint64_t)>& rob_accepts) {
  if (state.lookup_port_busy || state.rob_free == 0) {
    return nullptr;
  }
  // Hit-under-miss: allow lookup dispatch even when all walkers are busy,
  // provided an MSHR slot is available in case the bypassed lookup also misses.
  if (state.mshr_free <= state.lookup_in_flight) {
    return nullptr;
  }

  const size_t n = state.requester_queues.size();
  if (n == 0) return nullptr;

  for (size_t offset = 0; offset < n; ++offset) {
    const size_t src = (static_cast<size_t>(next_src_) + offset) % n;
    auto& q = state.requester_queues[src];
    if (!q.empty() && rob_accepts(q.front()->id)) {
      Txn* chosen = q.front();
      q.pop_front();
      next_src_ = static_cast<uint32_t>((src + 1) % n);
      return chosen;
    }
  }
  return nullptr;
}

std::unique_ptr<ArbiterPolicy> make_arbiter_policy(const std::string& name) {
  if (name == "InOrderFifo") return std::make_unique<InOrderFifoPolicy>();
  if (name == "RoundRobin") return std::make_unique<RoundRobinPolicy>();
  if (name == "IdAwareBypass") return std::make_unique<IdAwareBypassPolicy>();
  throw std::runtime_error("unknown arbiter policy: " + name);
}

}  // namespace perf
