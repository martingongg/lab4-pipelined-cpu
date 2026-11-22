#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "config.h"
#include "dtlb.h"
#include "json.h"
#include "memory.h"
#include "simulator.h"

namespace perf {
namespace {

SimConfig make_base_config() {
  SimConfig cfg;
  cfg.seed = 42;
  cfg.requesters = 4;
  cfg.requester_queue_depth = 8;
  cfg.policy = "InOrderFifo";
  cfg.dtlb.entries = 32;
  cfg.dtlb.ways = 32;
  cfg.dtlb.full_assoc = true;
  cfg.dtlb.hit_cycles = 1;
  cfg.dtlb.page_bytes = 4096;
  cfg.walker.count = 1;
  cfg.walker.levels = 3;
  cfg.walker.mshr_entries = 16;
  cfg.walker.coalesce_misses = true;
  cfg.memory.access_cycles = 40;
  cfg.retire.in_order = true;
  cfg.retire.rob_entries = 64;
  cfg.retire.retire_width = 4;
  cfg.workload.type = "zipf";
  cfg.workload.working_set_pages = 256;
  cfg.workload.transactions = 5000;
  return cfg;
}

TEST(JsonAndConfig, RoundTripHandoffDefaultSchema) {
  const std::string raw = R"({
    "seed": 1,
    "requesters": 4,
    "dtlb": { "entries": 32, "assoc": "full", "hit_cycles": 1, "page_bytes": 4096 },
    "walker": { "count": 1, "levels": 3 },
    "memory": { "access_cycles": 40 },
    "policy": "IdAwareBypass",
    "retire_in_order": true,
    "workload": { "type": "zipf", "working_set_pages": 256, "transactions": 200000 }
  })";
  JsonValue j = parse_json(raw);
  SimConfig cfg = SimConfig::from_json(j);
  EXPECT_EQ(cfg.seed, 1u);
  EXPECT_EQ(cfg.requesters, 4u);
  EXPECT_EQ(cfg.dtlb.entries, 32u);
  EXPECT_EQ(cfg.dtlb.ways, 32u);
  EXPECT_TRUE(cfg.dtlb.full_assoc);
  EXPECT_EQ(cfg.dtlb.hit_cycles, 1u);
  EXPECT_EQ(cfg.dtlb.page_bytes, 4096u);
  EXPECT_EQ(cfg.walker.count, 1u);
  EXPECT_EQ(cfg.walker.levels, 3u);
  EXPECT_EQ(cfg.memory.access_cycles, 40u);
  EXPECT_EQ(cfg.policy, "IdAwareBypass");
  EXPECT_TRUE(cfg.retire.in_order);
  EXPECT_EQ(cfg.workload.type, "zipf");
  EXPECT_EQ(cfg.workload.working_set_pages, 256u);
  EXPECT_EQ(cfg.workload.transactions, 200000u);

  // Round-trip through to_json -> dump -> parse_json -> from_json.
  SimConfig cfg2 = SimConfig::from_json(parse_json(cfg.to_json().dump(2)));
  EXPECT_EQ(cfg2.policy, cfg.policy);
  EXPECT_EQ(cfg2.workload.transactions, cfg.workload.transactions);
}

TEST(JsonAndConfig, RejectsInvalidConfigs) {
  SimConfig bad = make_base_config();
  bad.policy = "NonExistentPolicy";
  EXPECT_THROW(bad.validate(), std::runtime_error);

  bad = make_base_config();
  bad.dtlb.entries = 32;
  bad.dtlb.ways = 3;  // does not divide 32
  EXPECT_THROW(bad.validate(), std::runtime_error);

  bad = make_base_config();
  bad.dtlb.page_bytes = 5000;  // not a power of 2
  EXPECT_THROW(bad.validate(), std::runtime_error);
}

// Analytical Check 1: 100% hit workload gives exact latency == hit_cycles.
TEST(Analytical, AllHitsExactHitCycles) {
  for (uint64_t hit_cycles : {1ULL, 3ULL}) {
    SimConfig cfg = make_base_config();
    cfg.requesters = 1;
    cfg.requester_queue_depth = 1;
    cfg.policy = (hit_cycles == 1) ? "InOrderFifo" : "IdAwareBypass";
    cfg.dtlb.hit_cycles = hit_cycles;
    cfg.workload.type = "sequential";
    cfg.workload.working_set_pages = 8;  // Well within 32-entry DTLB
    cfg.workload.transactions = 500;

    RunOptions opts;
    opts.prewarm_dtlb = true;
    Simulator sim(cfg, opts);
    SimResults res = sim.run();

    EXPECT_EQ(res.transactions_completed, 500u);
    EXPECT_EQ(res.dtlb_hits, 500u);
    EXPECT_EQ(res.dtlb_misses, 0u);
    EXPECT_DOUBLE_EQ(res.dtlb_hit_rate, 1.0);
    EXPECT_DOUBLE_EQ(res.completion_latency.mean, static_cast<double>(hit_cycles));
    EXPECT_EQ(res.completion_latency.min, hit_cycles);
    EXPECT_EQ(res.completion_latency.max, hit_cycles);
    EXPECT_DOUBLE_EQ(res.retirement_latency.mean, static_cast<double>(hit_cycles));
    EXPECT_EQ(res.hol_stall_cycles, 0u);
  }
}

// Analytical Check 2: Single cold miss gives exact latency == hit_cycles + levels * access_cycles.
TEST(Analytical, SingleColdMissExactFormula) {
  struct Case {
    uint64_t hit;
    uint32_t levels;
    uint64_t mem;
  };
  const std::vector<Case> cases = {
      {1, 3, 40},  // Default Sv39-style: 1 + 3*40 = 121
      {2, 4, 25},  // ARMv8 4-level style: 2 + 4*25 = 102
      {1, 2, 10},  // 2-level: 1 + 2*10 = 21
  };
  for (const auto& tc : cases) {
    for (const std::string& pol : {"InOrderFifo", "RoundRobin", "IdAwareBypass"}) {
      SimConfig cfg = make_base_config();
      cfg.requesters = 1;
      cfg.policy = pol;
      cfg.dtlb.hit_cycles = tc.hit;
      cfg.walker.levels = tc.levels;
      cfg.memory.access_cycles = tc.mem;
      cfg.workload.type = "sequential";
      cfg.workload.working_set_pages = 1;
      cfg.workload.transactions = 1;

      Simulator sim(cfg);
      SimResults res = sim.run();

      const uint64_t expected = tc.hit + static_cast<uint64_t>(tc.levels) * tc.mem;
      EXPECT_EQ(res.transactions_completed, 1u);
      EXPECT_EQ(res.dtlb_hits, 0u);
      EXPECT_EQ(res.dtlb_misses, 1u);
      EXPECT_EQ(res.walks_completed, 1u);
      EXPECT_EQ(res.memory_requests, tc.levels);
      EXPECT_EQ(res.completion_latency.min, expected) << "policy=" << pol;
      EXPECT_EQ(res.completion_latency.max, expected) << "policy=" << pol;
      EXPECT_EQ(res.retirement_latency.min, expected) << "policy=" << pol;
      EXPECT_EQ(res.total_cycles, expected) << "policy=" << pol;
    }
  }
}

TEST(DtlbUnit, FullAndSetAssociativeLruEviction) {
  // 2-way, 4-entry DTLB -> 2 sets (even VPNs in set 0, odd VPNs in set 1).
  DtlbConfig cfg;
  cfg.entries = 4;
  cfg.ways = 2;
  cfg.full_assoc = false;
  cfg.page_bytes = 4096;
  Dtlb tlb(cfg);

  const uint64_t p0 = 0ULL * 4096;  // set 0
  const uint64_t p2 = 2ULL * 4096;  // set 0
  const uint64_t p4 = 4ULL * 4096;  // set 0
  const uint64_t p1 = 1ULL * 4096;  // set 1

  tlb.install(p0, 1, 0);
  tlb.install(p2, 2, 1);
  tlb.install(p1, 3, 2);
  // Touch p0 so p2 becomes LRU in set 0.
  EXPECT_TRUE(tlb.lookup_and_touch(p0, 4, 3));
  // Installing p4 into set 0 must evict p2, keeping p0 and p1.
  tlb.install(p4, 5, 4);

  EXPECT_TRUE(tlb.peek_hit(p0));
  EXPECT_FALSE(tlb.peek_hit(p2));
  EXPECT_TRUE(tlb.peek_hit(p4));
  EXPECT_TRUE(tlb.peek_hit(p1));
}

TEST(DtlbUnit, HugePage2MiBReachReducesMisses) {
  // 32 MiB byte footprint: 8192 x 4 KiB pages (thrashes 32-entry DTLB) vs 16 x 2 MiB pages (fits!).
  SimConfig cfg4k = make_base_config();
  cfg4k.policy = "IdAwareBypass";
  cfg4k.dtlb.entries = 32;
  cfg4k.dtlb.ways = 32;
  cfg4k.dtlb.page_bytes = 4096;
  cfg4k.workload.type = "kv_cache";
  cfg4k.workload.working_set_bytes = 32ULL * 1024 * 1024;
  cfg4k.workload.transactions = 4000;

  SimConfig cfg2m = cfg4k;
  cfg2m.dtlb.page_bytes = 2097152;  // 2 MiB

  SimResults r4k = Simulator(cfg4k).run();
  SimResults r2m = Simulator(cfg2m).run();

  EXPECT_GT(r2m.dtlb_hit_rate, r4k.dtlb_hit_rate + 0.05);
  EXPECT_LT(r2m.completion_latency.mean, r4k.completion_latency.mean);
}

TEST(MemoryUnit, BandwidthCapAddsDeterministicQueueingDelay) {
  MemoryConfig cfg;
  cfg.access_cycles = 10;
  cfg.max_requests_per_cycle = 1;
  MemoryModel mem(cfg);

  // Two simultaneous requests at cycle 0 -> req 0 finishes at 10, req 1 dispatches at 1 & finishes at 11.
  mem.submit(0x1000, 0, 0);
  mem.submit(0x2000, 0, 1);

  uint64_t done0 = 0, done1 = 0;
  for (uint64_t c = 1; c <= 20; ++c) {
    mem.tick(c);
    for (const auto& resp : mem.drain_completed()) {
      if (resp.walker_id == 0) done0 = c;
      if (resp.walker_id == 1) done1 = c;
    }
  }
  EXPECT_EQ(done0, 10u);
  EXPECT_EQ(done1, 11u);
}

// Invariant Checks: every transaction completes once; retire_in_order retires in exact ID order.
TEST(Invariants, SingleCompletionAndInOrderRetireAllPolicies) {
  for (const std::string& pol : {"InOrderFifo", "RoundRobin", "IdAwareBypass"}) {
    SimConfig cfg = make_base_config();
    cfg.policy = pol;
    cfg.retire.in_order = true;
    cfg.workload.type = "zipf";
    cfg.workload.working_set_pages = 128;
    cfg.workload.transactions = 2000;

    SimResults res = Simulator(cfg).run();
    EXPECT_EQ(res.transactions_completed, 2000u) << "policy=" << pol;
    EXPECT_EQ(res.dtlb_hits + res.dtlb_misses, 2000u) << "policy=" << pol;
    ASSERT_EQ(res.retired_id_order.size(), 2000u) << "policy=" << pol;

    for (uint64_t i = 0; i < 2000; ++i) {
      ASSERT_EQ(res.retired_id_order[static_cast<size_t>(i)], i)
          << "out-of-order retire under policy=" << pol;
    }
  }
}

// Invariant Check: RoundRobin prevents starvation under heavily asymmetric requester load.
TEST(Invariants, RoundRobinPreventsStarvation) {
  const std::string trace_path = "/tmp/perf_model_starvation_test.csv";
  {
    std::ofstream ofs(trace_path);
    ofs << "cycle,src,vaddr\n";
    // Requester 0 floods 120 cold-miss/hit requests at cycle 0, followed by 1 request from requester 1.
    for (int i = 0; i < 120; ++i) {
      ofs << "0,0,0x" << std::hex << (0x100000ULL + static_cast<uint64_t>(i % 4) * 4096ULL)
          << std::dec << "\n";
    }
    ofs << "0,1,0x200000\n";
  }

  SimConfig base = make_base_config();
  base.requesters = 2;
  base.requester_queue_depth = 200;
  base.retire.in_order = false;
  base.retire.rob_entries = 200;
  base.workload.type = "trace";
  base.workload.trace_path = trace_path;

  SimConfig fifo_cfg = base;
  fifo_cfg.policy = "InOrderFifo";
  SimResults fifo_res = Simulator(fifo_cfg).run();

  SimConfig rr_cfg = base;
  rr_cfg.policy = "RoundRobin";
  SimResults rr_res = Simulator(rr_cfg).run();

  std::remove(trace_path.c_str());

  // Under InOrderFifo, requester 1 waits behind all 120 of requester 0's transactions.
  // Under RoundRobin, requester 1 is granted immediately after requester 0's first transaction.
  ASSERT_EQ(fifo_res.per_requester.size(), 2u);
  ASSERT_EQ(rr_res.per_requester.size(), 2u);
  EXPECT_LT(rr_res.per_requester[1].max_retirement_latency,
            fifo_res.per_requester[1].max_retirement_latency);
  EXPECT_LE(rr_res.per_requester[1].max_retirement_latency, 250u);
}

TEST(Invariants, DeterministicRepeatableOutput) {
  SimConfig cfg = make_base_config();
  cfg.policy = "IdAwareBypass";
  cfg.workload.type = "tiled_gemm";
  cfg.workload.transactions = 3000;

  std::string out1 = Simulator(cfg).run().to_json().dump(2);
  std::string out2 = Simulator(cfg).run().to_json().dump(2);
  EXPECT_EQ(out1, out2);
}

TEST(Workloads, AllSevenBuiltInWorkloadsCompleteCleanly) {
  const std::vector<std::string> workloads = {
      "sequential", "strided", "random", "zipf", "pointer_chase", "tiled_gemm", "kv_cache",
  };
  for (const auto& wl : workloads) {
    SimConfig cfg = make_base_config();
    cfg.policy = "IdAwareBypass";
    cfg.workload.type = wl;
    cfg.workload.working_set_pages = 128;
    cfg.workload.transactions = 1500;

    SimResults res = Simulator(cfg).run();
    EXPECT_EQ(res.transactions_completed, 1500u) << "workload=" << wl;
    EXPECT_GT(res.total_cycles, 0u) << "workload=" << wl;
    EXPECT_GE(res.dtlb_hit_rate, 0.0) << "workload=" << wl;
    EXPECT_LE(res.dtlb_hit_rate, 1.0) << "workload=" << wl;
  }
}

TEST(Workloads, CommentedCsvTraceReplayWorks) {
  const std::string trace_path = "/tmp/perf_model_commented_trace_test.csv";
  {
    std::ofstream ofs(trace_path);
    ofs << "# Comment line above CSV header\n";
    ofs << "cycle,src,vaddr\n";
    ofs << "0,0,0x10000000\n";
    ofs << "0,1,0x10010000\n";
    ofs << "1,0,0x10000004\n";
    ofs << "2,1,0x10010008\n";
  }
  SimConfig cfg = make_base_config();
  cfg.policy = "IdAwareBypass";
  cfg.workload.type = "trace";
  cfg.workload.trace_path = trace_path;

  SimResults res = Simulator(cfg).run();
  std::remove(trace_path.c_str());

  EXPECT_EQ(res.transactions_completed, 4u);
  EXPECT_EQ(res.dtlb_hits + res.dtlb_misses, 4u);
}

// A trace may legitimately schedule its only request far in the future.
// That idle wait is not livelock: the cycle bound must cover it, and the
// request then completes with the standard cold-miss latency (H + L*M).
TEST(Workloads, SparseTraceWithFutureTimestampCompletes) {
  const std::string trace_path = "/tmp/perf_model_sparse_trace_test.csv";
  {
    std::ofstream ofs(trace_path);
    ofs << "1000001,0,0x1000\n";
  }
  SimConfig cfg = make_base_config();
  cfg.workload.type = "trace";
  cfg.workload.trace_path = trace_path;

  SimResults res = Simulator(cfg).run();
  std::remove(trace_path.c_str());

  EXPECT_EQ(res.transactions_completed, 1u);
  EXPECT_EQ(res.dtlb_hits + res.dtlb_misses, 1u);
  // Issues at cycle 1000001, cold miss costs H + L*M = 1 + 3*40 = 121 cycles.
  EXPECT_EQ(res.total_cycles, 1000001u + 121u);
}

// The GEMM problem (matrix dimension, IF footprint) and the KV-cache data
// region must not change with the translation page size: a fixed 32 MiB
// working set used to build dim 1664 at 4 KiB pages but dim 1616 at 2 MiB
// pages, so page-size comparisons measured different address workloads.
TEST(Workloads, LayoutIndependentOfPageSize) {
  {
    auto layout_at = [](uint64_t page_bytes) {
      SimConfig cfg = make_base_config();
      cfg.dtlb.page_bytes = page_bytes;
      cfg.workload.type = "tiled_gemm";
      cfg.workload.working_set_bytes = 32ull * 1024 * 1024;
      WorkloadGenerator gen(cfg);
      return std::pair<uint64_t, uint64_t>(gen.gemm_dim(), gen.gemm_if_bytes());
    };
    EXPECT_EQ(layout_at(4096), layout_at(2097152));
    EXPECT_EQ(layout_at(4096).first, 1664u);
    EXPECT_EQ(layout_at(4096).second, 16384u);
  }
  {
    auto layout_at = [](uint64_t page_bytes) {
      SimConfig cfg = make_base_config();
      cfg.dtlb.page_bytes = page_bytes;
      cfg.workload.type = "kv_cache";
      cfg.workload.working_set_bytes = 32ull * 1024 * 1024;
      WorkloadGenerator gen(cfg);
      return std::pair<uint64_t, uint64_t>(gen.kv_if_bytes(), gen.kv_data_bytes());
    };
    EXPECT_EQ(layout_at(4096), layout_at(2097152));
    EXPECT_EQ(layout_at(4096).first, 16384u);
    EXPECT_EQ(layout_at(4096).second, 32ull * 1024 * 1024 - 16384u);
  }
}

// Sanity Trend 1: Latency rises and hit rate drops as working set exceeds DTLB reach.
TEST(SanityTrends, LatencyRisesAboveDtlbReach) {
  SimConfig small_ws = make_base_config();
  small_ws.policy = "InOrderFifo";
  small_ws.workload.type = "zipf";
  small_ws.workload.working_set_pages = 16;  // Fits inside 32-entry DTLB
  small_ws.workload.transactions = 6000;

  SimConfig large_ws = small_ws;
  large_ws.workload.working_set_pages = 512;  // 16x DTLB reach

  SimResults r_small = Simulator(small_ws).run();
  SimResults r_large = Simulator(large_ws).run();

  EXPECT_GT(r_small.dtlb_hit_rate, r_large.dtlb_hit_rate);
  EXPECT_GT(r_large.completion_latency.mean, r_small.completion_latency.mean * 2.0);
}

// Sanity Trend 2: More walkers reduces HoL stall cycles under InOrderFifo.
TEST(SanityTrends, MoreWalkersReducesHolStallsUnderInOrderFifo) {
  SimConfig w1 = make_base_config();
  w1.policy = "InOrderFifo";
  w1.walker.count = 1;
  w1.workload.type = "zipf";
  w1.workload.working_set_pages = 256;
  w1.workload.transactions = 8000;

  SimConfig w2 = w1;
  w2.walker.count = 2;

  SimConfig w4 = w1;
  w4.walker.count = 4;

  SimResults r1 = Simulator(w1).run();
  SimResults r2 = Simulator(w2).run();
  SimResults r4 = Simulator(w4).run();

  EXPECT_GT(r1.hol_stall_cycles, r2.hol_stall_cycles);
  EXPECT_GT(r2.hol_stall_cycles, r4.hol_stall_cycles);
}

// Sanity Trend 3: IdAwareBypass cuts HoL stalls & completion latency vs InOrderFifo, at ROB cost.
TEST(SanityTrends, IdAwareBypassReducesHolBlockingAtRobCost) {
  SimConfig fifo = make_base_config();
  fifo.policy = "InOrderFifo";
  fifo.walker.count = 1;
  fifo.workload.type = "zipf";
  fifo.workload.working_set_pages = 256;
  fifo.workload.transactions = 10000;

  SimConfig bypass = fifo;
  bypass.policy = "IdAwareBypass";

  SimResults r_fifo = Simulator(fifo).run();
  SimResults r_bypass = Simulator(bypass).run();

  EXPECT_LT(r_bypass.hol_stall_cycles, r_fifo.hol_stall_cycles);
  EXPECT_LT(r_bypass.completion_latency.mean, r_fifo.completion_latency.mean);
  EXPECT_GT(r_bypass.rob_max_occupancy, r_fifo.rob_max_occupancy);
}

}  // namespace
}  // namespace perf
