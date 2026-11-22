#pragma once

#include <cstdint>
#include <vector>

#include "config.h"
#include "json.h"
#include "txn.h"

namespace perf {

struct LatencySummary {
  double mean = 0.0;
  uint64_t p50 = 0;
  uint64_t p95 = 0;
  uint64_t p99 = 0;
  uint64_t min = 0;
  uint64_t max = 0;
};

struct RequesterStats {
  uint32_t src = 0;
  uint64_t transactions = 0;
  uint64_t hits = 0;
  uint64_t misses = 0;
  double mean_completion_latency = 0.0;
  double mean_retirement_latency = 0.0;
  uint64_t max_retirement_latency = 0;
};

struct SimResults {
  SimConfig config;
  uint64_t total_cycles = 0;
  uint64_t transactions_completed = 0;
  double throughput_txns_per_cycle = 0.0;

  uint64_t dtlb_hits = 0;
  uint64_t dtlb_misses = 0;
  double dtlb_hit_rate = 0.0;

  uint64_t walks_started = 0;
  uint64_t walks_completed = 0;
  uint64_t coalesced_misses = 0;
  uint64_t memory_requests = 0;
  double walker_utilization = 0.0;

  uint64_t hol_stall_cycles = 0;
  double hol_stall_fraction = 0.0;
  uint64_t hol_txn_wait_cycles = 0;

  uint32_t rob_max_occupancy = 0;
  double rob_mean_occupancy = 0.0;

  LatencySummary completion_latency;
  LatencySummary retirement_latency;
  std::vector<RequesterStats> per_requester;

  // Exposed for unit-test invariant verification (not serialized to JSON).
  std::vector<uint64_t> retired_id_order;

  JsonValue to_json() const;
};

class StatsCollector {
 public:
  explicit StatsCollector(const SimConfig& cfg);

  void record_retired(const Txn& txn);
  void record_hol_cycle(uint64_t ready_hits_waiting);

  SimResults finalize(uint64_t total_cycles, uint64_t dtlb_hits, uint64_t dtlb_misses,
                      uint64_t walks_started, uint64_t walks_completed, uint64_t coalesced_misses,
                      uint64_t busy_walker_cycles, uint64_t memory_requests,
                      uint32_t rob_max_occupancy, uint64_t rob_occupancy_sum) const;

 private:
  static LatencySummary summarize(std::vector<uint64_t> samples);

  SimConfig cfg_;
  std::vector<uint64_t> comp_latencies_;
  std::vector<uint64_t> retire_latencies_;
  std::vector<uint64_t> retired_id_order_;

  struct ReqAccum {
    uint64_t txns = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t comp_sum = 0;
    uint64_t ret_sum = 0;
    uint64_t ret_max = 0;
  };
  std::vector<ReqAccum> per_req_;

  uint64_t hol_stall_cycles_ = 0;
  uint64_t hol_txn_wait_cycles_ = 0;
};

}  // namespace perf
