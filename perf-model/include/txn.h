#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace perf {

// Default semantic mapping of requester stream IDs to a pipelined CPU's memory ports.
enum class RequesterKind : uint32_t {
  InstructionFetch = 0,
  Load = 1,
  Store = 2,
  Prefetch = 3,
};

inline std::string requester_label(uint32_t src) {
  switch (src) {
    case 0: return "IF";
    case 1: return "Load";
    case 2: return "Store";
    case 3: return "Prefetch";
    default: return "Req" + std::to_string(src);
  }
}

struct Txn {
  uint64_t id = 0;               // Monotonic global transaction ID
  uint32_t src = 0;              // Requester stream ID
  uint64_t vaddr = 0;            // 64-bit virtual address
  uint64_t vpn = 0;              // Virtual page number (vaddr >> page_shift)
  uint64_t t_issue = 0;          // Cycle transaction entered requester queue
  uint64_t t_lookup = 0;         // Cycle transaction entered DTLB lookup stage
  uint64_t t_done = 0;           // Cycle translation completed (hit or walk finished)
  uint64_t t_retire = 0;         // Cycle transaction retired from ROB
  bool tlb_hit = false;          // True if initial DTLB lookup hit
  bool coalesced = false;        // True if miss merged onto an existing in-flight walk
  bool completed = false;
  bool retired = false;
  uint64_t hol_wait_cycles = 0;  // Cycles waited queued as a ready hit behind an active miss
};

struct TlbEvent {
  uint64_t cycle = 0;
  bool is_install = false;  // false = LOOKUP, true = INSTALL
  uint64_t txn_id = 0;
  uint64_t vaddr = 0;
  bool hit = false;
};

// State snapshot exposed to ArbiterPolicy::pick().
struct PipelineState {
  std::vector<std::deque<Txn*>>& requester_queues;
  uint32_t lookup_in_flight = 0;   // Transactions currently inside DTLB hit_cycles pipeline
  bool lookup_port_busy = false;   // True if a lookup was already launched this cycle
  uint32_t free_walkers = 0;       // Currently idle hardware page-table walkers
  uint32_t total_walkers = 0;
  uint32_t active_walks = 0;       // Busy walkers + queued MSHR entries
  uint32_t mshr_free = 0;          // Remaining MSHR slots (including free walkers)
  uint32_t rob_free = 0;           // Remaining ROB slots
  bool retire_in_order = true;
};

}  // namespace perf
