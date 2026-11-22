#pragma once

#include <cstdint>
#include <string>

#include "json.h"

namespace perf {

struct DtlbConfig {
  uint32_t entries = 32;
  uint32_t ways = 32;            // "full" resolves to entries
  bool full_assoc = true;
  uint64_t hit_cycles = 1;
  uint64_t page_bytes = 4096;    // 4 KiB default; 2 MiB (2097152) supported
};

struct WalkerConfig {
  uint32_t count = 1;
  uint32_t levels = 3;           // Sv39-style 3-level default; configurable (e.g. 4 for ARMv8)
  uint32_t mshr_entries = 16;    // Pending miss buffer capacity (used under IdAwareBypass)
  bool coalesce_misses = true;   // Merge concurrent misses to an already-walking VPN
};

struct MemoryConfig {
  uint64_t access_cycles = 40;
  uint32_t max_requests_per_cycle = 0;  // 0 = unlimited issue bandwidth
  uint32_t max_in_flight = 0;           // 0 = unlimited concurrency
};

struct RetireConfig {
  bool in_order = true;
  uint32_t rob_entries = 64;
  uint32_t retire_width = 4;     // Max retirements per cycle (0 = unlimited)
};

struct WorkloadConfig {
  std::string type = "zipf";     // sequential, strided, random, zipf, pointer_chase, tiled_gemm, kv_cache, trace
  uint64_t working_set_pages = 256;
  uint64_t working_set_bytes = 0;  // Optional: if > 0, sets byte footprint independent of page_bytes
  uint64_t transactions = 200000;
  uint64_t stride_bytes = 4096;  // Used by strided (sequential uses 64B cache-line default unless set)
  double zipf_alpha = 1.05;      // Zipfian power-law skew
  double kv_random_prob = 0.08;  // Random lookup fraction in kv_cache workload
  uint32_t gemm_tile = 32;       // Tile edge length for tiled_gemm
  bool pointer_chase_dependent = true;
  std::string trace_path;        // CSV trace path when type == "trace"
};

struct SimConfig {
  uint64_t seed = 1;
  uint32_t requesters = 4;
  uint32_t requester_queue_depth = 8;
  std::string policy = "IdAwareBypass";  // InOrderFifo, RoundRobin, IdAwareBypass
  DtlbConfig dtlb;
  WalkerConfig walker;
  MemoryConfig memory;
  RetireConfig retire;
  WorkloadConfig workload;

  void validate() const;
  JsonValue to_json() const;
  static SimConfig from_json(const JsonValue& root);
  static SimConfig from_file(const std::string& path);
};

}  // namespace perf
