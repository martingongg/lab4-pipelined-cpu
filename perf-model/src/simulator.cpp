#include "simulator.h"

#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace perf {

Simulator::Simulator(const SimConfig& cfg, const RunOptions& opts)
    : cfg_(cfg),
      opts_(opts),
      dtlb_(cfg.dtlb, !opts.dump_tlb_events_path.empty()),
      memory_(cfg.memory),
      walker_(cfg.walker),
      arbiter_(make_arbiter_policy(cfg.policy)),
      retire_(cfg.retire),
      workload_(cfg),
      stats_(cfg),
      requester_queues_(cfg.requesters),
      in_flight_per_src_(cfg.requesters, 0) {
  cfg_.validate();
  if (opts_.prewarm_dtlb) {
    prewarm_working_set();
  }
}

void Simulator::prewarm_working_set() {
  constexpr uint64_t kBaseVirtualAddr = 0x0000100000000000ULL;
  const uint64_t pages = std::min<uint64_t>(cfg_.workload.working_set_pages, cfg_.dtlb.entries);
  for (uint64_t p = 0; p < pages; ++p) {
    dtlb_.install(kBaseVirtualAddr + p * cfg_.dtlb.page_bytes, 0, 0);
  }
}

bool Simulator::all_queues_empty() const {
  for (const auto& q : requester_queues_) {
    if (!q.empty()) return false;
  }
  return true;
}

bool Simulator::all_done() const {
  return !workload_.has_more() && all_queues_empty() && dtlb_.empty() && walker_.empty() &&
         memory_.empty() && retire_.empty();
}

void Simulator::issue_requesters(uint64_t cycle) {
  if (!workload_.has_more()) return;

  if (workload_.is_trace_mode()) {
    // Issue consecutive trace entries ready at `cycle` while target requester has buffer credits.
    bool progressed = true;
    while (progressed && workload_.has_more()) {
      progressed = false;
      for (uint32_t r = 0; r < cfg_.requesters; ++r) {
        if (in_flight_per_src_[r] >= cfg_.requester_queue_depth) continue;
        GeneratedReq req;
        if (workload_.next_for_src(r, cycle, in_flight_per_src_[r], req)) {
          Txn t;
          t.id = next_txn_id_++;
          t.src = req.src;
          t.vaddr = req.vaddr;
          t.vpn = dtlb_.vaddr_to_vpn(req.vaddr);
          t.t_issue = cycle;
          txn_storage_.push_back(t);
          requester_queues_[r].push_back(&txn_storage_.back());
          ++in_flight_per_src_[r];
          progressed = true;
          break;
        }
      }
    }
    return;
  }

  // Rotate starting requester each cycle so ID assignment across concurrent streams is unbiased.
  // Each requester holds its buffer credit from issue until retirement (apples-to-apples concurrency).
  for (uint32_t offset = 0; offset < cfg_.requesters && workload_.has_more(); ++offset) {
    const uint32_t r = (issue_rr_cursor_ + offset) % cfg_.requesters;
    if (in_flight_per_src_[r] >= cfg_.requester_queue_depth) continue;
    GeneratedReq req;
    if (workload_.next_for_src(r, cycle, in_flight_per_src_[r], req)) {
      Txn t;
      t.id = next_txn_id_++;
      t.src = req.src;
      t.vaddr = req.vaddr;
      t.vpn = dtlb_.vaddr_to_vpn(req.vaddr);
      t.t_issue = cycle;
      txn_storage_.push_back(t);
      requester_queues_[r].push_back(&txn_storage_.back());
      ++in_flight_per_src_[r];
    }
  }
  issue_rr_cursor_ = (issue_rr_cursor_ + 1) % cfg_.requesters;
}

void Simulator::write_trace_dump() const {
  if (opts_.dump_trace_path.empty()) return;
  std::ofstream ofs(opts_.dump_trace_path);
  if (!ofs) {
    throw std::runtime_error("failed to open trace dump path: " + opts_.dump_trace_path);
  }
  ofs << "id,src,vaddr,t_issue,t_lookup,t_done,t_retire,tlb_hit\n";
  for (const auto& t : txn_storage_) {
    ofs << t.id << "," << t.src << ",0x" << std::hex << t.vaddr << std::dec << "," << t.t_issue
        << "," << t.t_lookup << "," << t.t_done << "," << t.t_retire << ","
        << (t.tlb_hit ? 1 : 0) << "\n";
  }
}

void Simulator::write_tlb_events_dump() const {
  if (opts_.dump_tlb_events_path.empty()) return;
  std::ofstream ofs(opts_.dump_tlb_events_path);
  if (!ofs) {
    throw std::runtime_error("failed to open TLB events dump path: " + opts_.dump_tlb_events_path);
  }
  ofs << "cycle,event,txn_id,vaddr,hit\n";
  for (const auto& ev : dtlb_.events()) {
    ofs << ev.cycle << "," << (ev.is_install ? "INSTALL" : "LOOKUP") << "," << ev.txn_id << ",0x"
        << std::hex << ev.vaddr << std::dec << "," << (ev.hit ? 1 : 0) << "\n";
  }
}

SimResults Simulator::run() {
  std::vector<Txn*> walker_done;
  std::vector<Txn*> dtlb_hits;
  std::vector<Txn*> dtlb_misses;
  std::vector<Txn*> retired_now;

  uint64_t cycle = 0;
  // Guard against accidental livelock bugs in tests or invalid custom configs.
  const uint64_t max_cycles =
      std::max<uint64_t>(1000000ULL, workload_.total_transactions() *
                                         (cfg_.dtlb.hit_cycles +
                                          cfg_.walker.levels * cfg_.memory.access_cycles + 16) *
                                         2);

  for (; cycle <= max_cycles; ++cycle) {
    walker_done.clear();
    dtlb_hits.clear();
    dtlb_misses.clear();
    retired_now.clear();

    // 1. Memory responses completing at `cycle`.
    memory_.tick(cycle);

    // 2. Page-table walkers advance (issue next level or install into DTLB & finish at `cycle`).
    walker_.tick(cycle, memory_, dtlb_, walker_done);
    for (Txn* t : walker_done) {
      retire_.on_complete(t);
    }

    // 3. DTLB lookups resolving at `cycle` (hits complete immediately; misses enter walker/MSHR).
    dtlb_.tick(cycle, dtlb_hits, dtlb_misses);
    for (Txn* t : dtlb_hits) {
      retire_.on_complete(t);
    }
    for (Txn* t : dtlb_misses) {
      walker_.on_miss(t, cycle, memory_);
    }

    // 4. Retire completed transactions (strictly in ID order if retire_in_order) so freed
    // ROB slots and requester credits become available to new work in `cycle`.
    retire_.tick(cycle, retired_now);
    for (Txn* t : retired_now) {
      if (t->src < in_flight_per_src_.size() && in_flight_per_src_[t->src] > 0) {
        --in_flight_per_src_[t->src];
      }
      stats_.record_retired(*t);
    }

    if (all_done()) {
      break;
    }

    // 5. Requesters inject new transactions into queues with available outstanding credits.
    issue_requesters(cycle);

    // 6. Arbiter selects up to one queued transaction to launch into DTLB lookup at `cycle`.
    PipelineState state{
        requester_queues_,
        dtlb_.in_flight(),
        false,
        walker_.free_walkers(),
        walker_.total_walkers(),
        walker_.active_walks(),
        walker_.mshr_capacity_free(),
        retire_.rob_free(),
        cfg_.retire.in_order,
    };
    Txn* granted = arbiter_->pick(cycle, state, [this](uint64_t id) {
      return retire_.can_accept(id);
    });
    if (granted != nullptr) {
      retire_.on_dispatch(granted);
      dtlb_.start_lookup(granted, cycle);
    }

    // 7. Sample per-cycle occupancy, walker utilization, and HoL stall cycles.
    walker_.sample_cycle();
    retire_.sample_cycle();
    if (granted == nullptr && walker_.active_walks() > 0) {
      uint64_t ready_hits_waiting = 0;
      for (const auto& q : requester_queues_) {
        for (Txn* t : q) {
          if (dtlb_.peek_hit(t->vaddr)) {
            ++t->hol_wait_cycles;
            ++ready_hits_waiting;
          }
        }
      }
      stats_.record_hol_cycle(ready_hits_waiting);
    }
  }

  if (!all_done()) {
    throw std::runtime_error("simulation exceeded max_cycles safety limit (possible livelock)");
  }

  write_trace_dump();
  write_tlb_events_dump();

  return stats_.finalize(cycle, dtlb_.hits(), dtlb_.misses(), walker_.walks_started(),
                         walker_.walks_completed(), walker_.coalesced_misses(),
                         walker_.busy_walker_cycles(), memory_.total_requests(),
                         retire_.max_occupancy(), retire_.occupancy_sum());
}

}  // namespace perf
