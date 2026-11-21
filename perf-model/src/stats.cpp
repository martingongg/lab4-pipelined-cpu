#include "stats.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace perf {

namespace {

JsonValue latency_to_json(const LatencySummary& s) {
  JsonValue j = JsonValue::make_object();
  j.set("mean", s.mean);
  j.set("p50", s.p50);
  j.set("p95", s.p95);
  j.set("p99", s.p99);
  j.set("min", s.min);
  j.set("max", s.max);
  return j;
}

uint64_t nearest_rank_percentile(const std::vector<uint64_t>& sorted, double p) {
  if (sorted.empty()) return 0;
  const double rank = std::ceil((p / 100.0) * static_cast<double>(sorted.size()));
  size_t idx = (rank <= 1.0) ? 0 : static_cast<size_t>(rank) - 1;
  if (idx >= sorted.size()) idx = sorted.size() - 1;
  return sorted[idx];
}

}  // namespace

JsonValue SimResults::to_json() const {
  JsonValue root = JsonValue::make_object();
  root.set("config", config.to_json());

  JsonValue m = JsonValue::make_object();
  m.set("total_cycles", total_cycles);
  m.set("transactions_completed", transactions_completed);
  m.set("throughput_txns_per_cycle", throughput_txns_per_cycle);
  m.set("dtlb_hits", dtlb_hits);
  m.set("dtlb_misses", dtlb_misses);
  m.set("dtlb_hit_rate", dtlb_hit_rate);
  m.set("walks_started", walks_started);
  m.set("walks_completed", walks_completed);
  m.set("coalesced_misses", coalesced_misses);
  m.set("memory_requests", memory_requests);
  m.set("walker_utilization", walker_utilization);
  m.set("hol_stall_cycles", hol_stall_cycles);
  m.set("hol_stall_fraction", hol_stall_fraction);
  m.set("hol_txn_wait_cycles", hol_txn_wait_cycles);
  m.set("rob_max_occupancy", rob_max_occupancy);
  m.set("rob_mean_occupancy", rob_mean_occupancy);
  m.set("completion_latency", latency_to_json(completion_latency));
  m.set("retirement_latency", latency_to_json(retirement_latency));

  JsonValue req_arr = JsonValue::make_array();
  for (const auto& rs : per_requester) {
    JsonValue jr = JsonValue::make_object();
    jr.set("src", rs.src);
    jr.set("label", requester_label(rs.src));
    jr.set("transactions", rs.transactions);
    jr.set("hits", rs.hits);
    jr.set("misses", rs.misses);
    jr.set("mean_completion_latency", rs.mean_completion_latency);
    jr.set("mean_retirement_latency", rs.mean_retirement_latency);
    jr.set("max_retirement_latency", rs.max_retirement_latency);
    req_arr.push_back(std::move(jr));
  }
  m.set("per_requester", std::move(req_arr));
  root.set("metrics", std::move(m));
  return root;
}

StatsCollector::StatsCollector(const SimConfig& cfg)
    : cfg_(cfg), per_req_(cfg.requesters) {
  if (cfg.workload.type != "trace") {
    comp_latencies_.reserve(static_cast<size_t>(cfg.workload.transactions));
    retire_latencies_.reserve(static_cast<size_t>(cfg.workload.transactions));
    retired_id_order_.reserve(static_cast<size_t>(cfg.workload.transactions));
  }
}

void StatsCollector::record_retired(const Txn& txn) {
  const uint64_t comp_lat = txn.t_done - txn.t_issue;
  const uint64_t ret_lat = txn.t_retire - txn.t_issue;
  comp_latencies_.push_back(comp_lat);
  retire_latencies_.push_back(ret_lat);
  retired_id_order_.push_back(txn.id);

  if (txn.src < per_req_.size()) {
    auto& acc = per_req_[txn.src];
    ++acc.txns;
    if (txn.tlb_hit) {
      ++acc.hits;
    } else {
      ++acc.misses;
    }
    acc.comp_sum += comp_lat;
    acc.ret_sum += ret_lat;
    acc.ret_max = std::max(acc.ret_max, ret_lat);
  }
}

void StatsCollector::record_hol_cycle(uint64_t ready_hits_waiting) {
  if (ready_hits_waiting > 0) {
    ++hol_stall_cycles_;
    hol_txn_wait_cycles_ += ready_hits_waiting;
  }
}

LatencySummary StatsCollector::summarize(std::vector<uint64_t> samples) {
  LatencySummary out;
  if (samples.empty()) return out;
  std::sort(samples.begin(), samples.end());
  const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
  out.mean = sum / static_cast<double>(samples.size());
  out.min = samples.front();
  out.max = samples.back();
  out.p50 = nearest_rank_percentile(samples, 50.0);
  out.p95 = nearest_rank_percentile(samples, 95.0);
  out.p99 = nearest_rank_percentile(samples, 99.0);
  return out;
}

SimResults StatsCollector::finalize(uint64_t total_cycles, uint64_t dtlb_hits, uint64_t dtlb_misses,
                                    uint64_t walks_started, uint64_t walks_completed,
                                    uint64_t coalesced_misses, uint64_t busy_walker_cycles,
                                    uint64_t memory_requests, uint32_t rob_max_occupancy,
                                    uint64_t rob_occupancy_sum) const {
  SimResults r;
  r.config = cfg_;
  r.total_cycles = total_cycles;
  r.transactions_completed = comp_latencies_.size();
  r.throughput_txns_per_cycle =
      (total_cycles > 0) ? static_cast<double>(r.transactions_completed) /
                               static_cast<double>(total_cycles)
                         : 0.0;
  r.dtlb_hits = dtlb_hits;
  r.dtlb_misses = dtlb_misses;
  const uint64_t total_lookups = dtlb_hits + dtlb_misses;
  r.dtlb_hit_rate =
      (total_lookups > 0) ? static_cast<double>(dtlb_hits) / static_cast<double>(total_lookups)
                          : 0.0;

  r.walks_started = walks_started;
  r.walks_completed = walks_completed;
  r.coalesced_misses = coalesced_misses;
  r.memory_requests = memory_requests;
  const double denom = static_cast<double>(total_cycles) * static_cast<double>(cfg_.walker.count);
  r.walker_utilization = (denom > 0.0) ? static_cast<double>(busy_walker_cycles) / denom : 0.0;

  r.hol_stall_cycles = hol_stall_cycles_;
  r.hol_stall_fraction =
      (total_cycles > 0) ? static_cast<double>(hol_stall_cycles_) / static_cast<double>(total_cycles)
                         : 0.0;
  r.hol_txn_wait_cycles = hol_txn_wait_cycles_;

  r.rob_max_occupancy = rob_max_occupancy;
  r.rob_mean_occupancy =
      (total_cycles > 0)
          ? static_cast<double>(rob_occupancy_sum) / static_cast<double>(total_cycles)
          : 0.0;

  r.completion_latency = summarize(comp_latencies_);
  r.retirement_latency = summarize(retire_latencies_);
  r.retired_id_order = retired_id_order_;

  r.per_requester.reserve(per_req_.size());
  for (uint32_t s = 0; s < per_req_.size(); ++s) {
    const auto& acc = per_req_[s];
    RequesterStats rs;
    rs.src = s;
    rs.transactions = acc.txns;
    rs.hits = acc.hits;
    rs.misses = acc.misses;
    rs.mean_completion_latency =
        (acc.txns > 0) ? static_cast<double>(acc.comp_sum) / static_cast<double>(acc.txns) : 0.0;
    rs.mean_retirement_latency =
        (acc.txns > 0) ? static_cast<double>(acc.ret_sum) / static_cast<double>(acc.txns) : 0.0;
    rs.max_retirement_latency = acc.ret_max;
    r.per_requester.push_back(rs);
  }
  return r;
}

}  // namespace perf
