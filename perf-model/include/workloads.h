#pragma once

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "config.h"

namespace perf {

struct GeneratedReq {
  uint32_t src = 0;
  uint64_t vaddr = 0;
  uint64_t earliest_cycle = 0;
};

// Deterministic address-pattern generator for synthetic, ML-shaped, and trace workloads.
class WorkloadGenerator {
 public:
  explicit WorkloadGenerator(const SimConfig& cfg);

  // Returns true if more transactions remain to be generated.
  bool has_more() const;

  // Attempt to generate the next request for requester `src` at `cycle`.
  // `in_flight_for_src` is used by dependent pointer-chasing workloads.
  bool next_for_src(uint32_t src, uint64_t cycle, uint32_t in_flight_for_src,
                    GeneratedReq& out);

  // Total transactions that will be issued by this workload.
  uint64_t total_transactions() const { return total_txns_; }
  uint64_t issued_count() const { return issued_; }

  // Latest earliest_cycle across trace requests (0 for generated workloads).
  // Sparse traces schedule work far in the future; the simulator's cycle
  // bound must account for that idle wait or it trips its livelock guard.
  uint64_t last_scheduled_cycle() const { return max_earliest_cycle_; }

  // True if trace mode dictates explicit per-requester queues.
  bool is_trace_mode() const { return cfg_.workload.type == "trace"; }

  // Layout accessors for tests. The GEMM problem (dim, IF footprint) and the
  // KV-cache data region must be identical across translation page sizes for
  // a fixed working-set size; only the translation geometry may change.
  uint64_t gemm_dim() const { return gemm_.dim; }
  uint64_t gemm_if_bytes() const { return gemm_.if_bytes; }
  uint64_t kv_if_bytes() const { return kv_if_bytes_; }
  uint64_t kv_data_bytes() const { return kv_data_bytes_; }

 private:
  struct GemmState {
    uint64_t base_if = 0;
    uint64_t if_bytes = 16384;  // Fixed instruction footprint (see init_tiled_gemm).
    uint64_t base_a = 0;
    uint64_t base_b = 0;
    uint64_t base_c = 0;
    uint64_t dim = 64;
    uint64_t tile = 32;
    uint64_t ii = 0, kk = 0, jj = 0;
    uint64_t i = 0, k = 0, j = 0;
    uint64_t if_cursor = 0;
    uint32_t single_req_phase = 0;
  };

  void init_zipf();
  void init_pointer_chase();
  void init_tiled_gemm();
  void init_kv_cache();
  void init_trace();

  uint64_t next_addr_for_src(uint32_t src);
  uint64_t next_gemm_addr(uint32_t src);
  uint64_t next_kv_addr(uint32_t src);

  SimConfig cfg_;
  uint64_t page_bytes_ = 4096;
  uint64_t footprint_bytes_ = 0;
  uint64_t num_pages_ = 1;
  uint64_t total_txns_ = 0;
  uint64_t issued_ = 0;

  std::mt19937_64 rng_;

  // Per-requester byte offsets (sequential / strided / kv_cache)
  std::vector<uint64_t> stream_offsets_;

  // Zipfian precomputed CDF
  std::vector<double> zipf_cdf_;

  // Pointer chase single-cycle permutation
  std::vector<uint64_t> chase_perm_;
  std::vector<uint64_t> chase_pos_;

  // Tiled GEMM loop-nest state
  GemmState gemm_;

  // KV-cache layout
  uint64_t kv_if_bytes_ = 16384;  // Fixed instruction footprint (see init_kv_cache).
  uint64_t kv_data_base_ = 0;
  uint64_t kv_data_bytes_ = 4096;

  // Trace replay buffer
  std::vector<GeneratedReq> trace_reqs_;
  size_t trace_idx_ = 0;
  uint64_t max_earliest_cycle_ = 0;
};

}  // namespace perf
