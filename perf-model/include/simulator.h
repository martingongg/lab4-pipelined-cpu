#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "arbiter.h"
#include "config.h"
#include "dtlb.h"
#include "memory.h"
#include "retire.h"
#include "stats.h"
#include "txn.h"
#include "walker.h"
#include "workloads.h"

namespace perf {

struct RunOptions {
  std::string dump_trace_path;       // Optional CSV of issued transactions (id,src,vaddr)
  std::string dump_tlb_events_path;  // Optional CSV of DTLB lookup/install events
  bool prewarm_dtlb = false;         // Optional test helper: preload working set into DTLB
};

class Simulator {
 public:
  explicit Simulator(const SimConfig& cfg, const RunOptions& opts = {});

  SimResults run();

 private:
  void prewarm_working_set();
  void issue_requesters(uint64_t cycle);
  bool all_queues_empty() const;
  bool all_done() const;
  void write_trace_dump() const;
  void write_tlb_events_dump() const;

  SimConfig cfg_;
  RunOptions opts_;
  Dtlb dtlb_;
  MemoryModel memory_;
  PageTableWalker walker_;
  std::unique_ptr<ArbiterPolicy> arbiter_;
  RetireStage retire_;
  WorkloadGenerator workload_;
  StatsCollector stats_;

  // Owns all transactions created during the run without invalidating raw Txn* pointers.
  std::deque<Txn> txn_storage_;
  std::vector<std::deque<Txn*>> requester_queues_;
  std::vector<uint32_t> in_flight_per_src_;
  uint64_t next_txn_id_ = 0;
  uint32_t issue_rr_cursor_ = 0;
};

}  // namespace perf
