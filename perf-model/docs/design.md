# Design Specification — Translation-Path Performance Model

## 1. Purpose and Scope

This document defines the microarchitecture, cycle-level timing semantics, arbitration policies, workload generators, and modeling boundaries of the **C++17 Translation-Path Performance Model** (`perf-model/`).

In the parent repository (`lab4-pipelined-cpu`), the SystemVerilog RTL implements a 64-bit, 5-stage pipelined ARM LEGv8 integer core (`IF → ID → EX → MEM → WB`) backed by ideal, single-cycle physical memories (`instructmem.sv`, `datamem.sv`). This performance model explores what happens when a virtual-memory translation path—specifically a **Data/Unified TLB (DTLB)**, **multi-level hardware page-table walker**, **request arbiter**, **memory latency model**, and **reorder/retire stage**—sits between the core's memory requesters (`IF`, `Load`, `Store`, `Prefetch`) and physical memory.

> All parameters, latencies, and microarchitectural structures in this model are **generic, illustrative, and based solely on publicly available computer-architecture references**.

---

## 2. Pipeline Architecture

```
  Requesters (0..R-1)          Arbiter Policy            DTLB Lookup
  ┌──────────────────┐        ┌───────────────┐        ┌───────────────────┐
  │ 0: IF            │───────▶│ InOrderFifo   │───────▶│ Full / N-way LRU  │
  │ 1: Load  (LDUR)  │───────▶│ RoundRobin    │  ≤1/cy │ hit_cycles (1)    │
  │ 2: Store (STUR)  │───────▶│ IdAwareBypass │        │ 4 KiB / 2 MiB     │
  │ 3: Prefetch      │───────▶│               │        └────┬─────────┬────┘
  └──────────────────┘        └───────────────┘             │ Hit     │ Miss
     ▲                                                      │         ▼
     │ release credit on retire                             │  ┌──────────────────┐
     │                                                      │  │ Page-Table Walker│
  ┌──────────────────┐                                      │  │ W walkers, L lvls│
  │ Retire Stage     │◀─────────────────────────────────────┘  │ MSHR + coalescing│
  │ In-Order ID ROB  │◀────────────── walk complete ───────────┤                  │
  │ or Out-of-Order  │                                         └────────▲─────────┘
  └──────────────────┘                                                  │ L × access_cycles
                                                               ┌────────▼─────────┐
                                                               │   Memory Model   │
                                                               │ Fixed + opt. BW  │
                                                               └──────────────────┘
```

### 2.1 Cycle-Driven Main Loop

The simulator is cycle-driven (`Simulator::run()` in `src/simulator.cpp`) with zero global state and deterministic seeded RNG (`std::mt19937_64`). Within each cycle `c`, stages resolve completions **before** issuing new work so handoffs between stages incur zero artificial bubbles:

```cpp
for (uint64_t c = 0; !all_done(); ++c) {
  memory_.tick(c);      // 1. Complete memory reads finishing at cycle c
  walker_.tick(c);      // 2. Step walks (next level read, or fill DTLB & mark t_done = c)
  dtlb_.tick(c);        // 3. Resolve lookups finishing at cycle c (hit -> retire; miss -> walker)
  retire_.tick(c);      // 4. Retire ready txns at c (frees ROB slots and requester credits)
  issue_requesters(c);  // 5. Inject new txns (t_issue = c) while requester credits remain
  arbiter_.pick(c);     // 6. Grant <= 1 txn into DTLB lookup (t_lookup = c)
  sample_cycle(c);      // 7. Sample walker utilization, ROB occupancy, and HoL stall cycles
}
```

Why **cycle-driven** rather than event-driven?
- A translation pipeline with a single DTLB lookup port, finite walker slots, ROB occupancy, and per-cycle HoL stall sampling naturally transitions on clock edges.
- Cycle-stepping mirrors RTL clocking directly, making cycle-by-cycle accounting (`walker_utilization`, `rob_mean_occupancy`, `hol_stall_cycles`) straightforward to reason about and correlate against future RTL modules.
- The tradeoff is runtime proportional to total simulated cycles (rather than number of events); in compiled C++17 (`-O2`), 100k–200k transactions run in tens of milliseconds, making cycle-driven execution fast enough for multi-hundred-point parameter sweeps.

---

## 3. Component Specifications

### 3.1 Requesters (`include/workloads.h`, `src/simulator.cpp`)
- Default `requesters = 4`, semantically mapped to `0: IF`, `1: Load`, `2: Store`, `3: Prefetch`.
- Each requester owns a bounded outstanding credit limit `requester_queue_depth` (default `8`, modeling finite IFQ/LSQ/prefetch-buffer capacity).
- A requester holds its credit from **issue (`t_issue`) until retirement (`t_retire`)**. Holding credits until retirement ensures apples-to-apples total outstanding concurrency ($L_{\max} = R \times \text{depth} = 32$) across all three arbitration policies, preventing artificial closed-loop bufferbloat.
- Global transaction IDs (`0, 1, 2, ...`) are assigned monotonically at issue using a rotating requester cursor each cycle for unbiased ordering.

### 3.2 DTLB (`include/dtlb.h`, `src/dtlb.cpp`)
- Configurable `entries` (default `32`), associativity (`"full"` or N-way power-of-2 set-associative), `hit_cycles` (default `1`), and `page_bytes` (`4096` for 4 KiB default; `2097152` for 2 MiB hugepages; any power-of-two $\ge 4096$).
- Replacement policy: **exact true-LRU** per set tracked with a monotonic sequence counter (`lru_seq_`). Invalid ways are populated in index order `0 .. ways-1` before any eviction occurs.
- Single-ported lookup pipeline: accepts at most 1 lookup per cycle; a lookup launched at `t_lookup` resolves at `t_lookup + hit_cycles`.
- Exposes a non-mutating `peek_hit(vaddr)` probe used exclusively by the statistics collector to measure head-of-line blocking (never used by the arbiter to make oracle decisions).

### 3.3 Page-Table Walker (`include/walker.h`, `src/walker.cpp`)
- Configurable hardware walker count (`walker.count`, default `1`) and translation levels (`walker.levels`, default `3`, modeled after RISC-V Sv39 39-bit virtual addressing; configurable to `4` for ARMv8-A 48-bit 4 KiB granule walks).
- Each walk performs `levels` dependent sequential memory reads. When level `k < levels` completes at cycle `c`, level `k + 1` is immediately submitted to `MemoryModel` at cycle `c`. When level `levels` completes at `c`, the walker installs the VPN into the DTLB at `c`, marks all attached transactions `t_done = c`, frees the walker slot, and immediately dispatches the next queued MSHR miss (if any).
- **MSHR & Miss Coalescing:** Under non-blocking `IdAwareBypass`, misses arriving while all walkers are busy enter a finite MSHR queue (`mshr_entries`, default `16`). When `coalesce_misses = true` (default), secondary misses targeting an already-walking or MSHR-queued VPN attach to that existing walk rather than launching a duplicate walk.

### 3.4 Memory Model (`include/memory.h`, `src/memory.cpp`)
- Fixed round-trip access latency (`memory.access_cycles`, default `40` cycles).
- Optional issue bandwidth limit (`max_requests_per_cycle`, `0` = unlimited) and concurrency limit (`max_in_flight`, `0` = unlimited). Excess requests wait in an internal FIFO queue before starting their `access_cycles` latency countdown.

### 3.5 Retire Stage / ROB (`include/retire.h`, `src/retire.cpp`)
- Configurable `retire_in_order` (default `true`), `rob_entries` (default `64`), and `retire_width` (default `4` txns/cycle; `0` = unlimited).
- When `retire_in_order = true`, the ROB enforces a sliding ID window `[next_retire_id, next_retire_id + rob_entries)`. Only transactions inside this window may enter the pipeline, guaranteeing deadlock-free in-order retirement by global transaction ID.
- When `retire_in_order = false`, any completed transaction retires immediately (up to `retire_width`/cycle), modeling a non-blocking downstream memory system that consumes translations out of order.

---

## 4. Formal Arbitration Policy Definitions (`include/arbiter.h`, `src/arbiter.cpp`)

All three policies share the same single-ported DTLB lookup constraint (`≤ 1` grant/cycle) and ROB acceptance check, differing strictly in **queue selection** and **miss-blocking behavior**:

| Policy | Queue Selection | Walker Blocking Rule | Architectural Behavior |
|---|---|---|---|
| **`InOrderFifo`** | Selects the strictly oldest queued transaction across all requesters (`min(txn.id)`). | **Conservative walker reservation:** grants only when `free_walkers > lookup_in_flight`. | With `walker.count = 1`, the head transaction must complete its lookup (and walk, on a miss) before the next transaction can enter the DTLB. Maximizes HoL blocking behind misses. Increasing `walker.count` allows hits behind up to `W - 1` walks. |
| **`RoundRobin`** | Rotates grant priority across per-requester queues `0 .. R-1`, advancing `next_src = (granted + 1) % R`. | **Conservative walker reservation:** grants only when `free_walkers > lookup_in_flight`. | Prevents one bursty requester from starving quieter requesters (bounded wait), while still blocking behind shared-walker contention when all walkers are busy. |
| **`IdAwareBypass`** | Rotates fairly across per-requester queues `0 .. R-1` (same fairness cursor as `RoundRobin`). | **Non-blocking hit-under-miss:** grants even when `free_walkers == 0`, gated only on `mshr_capacity_free > lookup_in_flight` and ROB window space. | Subsequent DTLB hits resolve and complete immediately (`t_done = t_lookup + hit_cycles`) while earlier misses walk. Subsequent misses coalesce or queue in MSHR. Under `retire_in_order = true`, completed hits buffer in the ROB until older misses finish (quantifying ROB occupancy cost). |

---

## 5. Metric Definitions (`include/stats.h`, `src/stats.cpp`)

- **Completion latency:** `t_done - t_issue` (cycles from entering the requester queue until address translation finishes). Reported with `mean`, `p50`, `p95`, `p99`, `min`, and `max`.
- **Retirement latency:** `t_retire - t_issue` (cycles from issue until retirement from the ROB). Identical to completion latency under `InOrderFifo` (`walkers = 1`) and `retire_in_order = false`; diverges from completion latency under `IdAwareBypass` + `retire_in_order = true` because hits complete early and wait in the ROB.
- **Throughput:** `transactions_completed / total_cycles`.
- **DTLB hit rate:** `dtlb_hits / (dtlb_hits + dtlb_misses)`.
- **Walker utilization:** `busy_walker_cycles / (total_cycles * walker.count)`.
- **HoL stall cycles (`hol_stall_cycles`):** Number of cycles in which (a) the arbiter granted **no** transaction into the DTLB lookup port, (b) at least one page-table walk was active (`active_walks > 0`), and (c) at least one queued waiting transaction would have hit in the DTLB (`dtlb.peek_hit(vaddr) == true`). Also reports `hol_stall_fraction = hol_stall_cycles / total_cycles` and `hol_txn_wait_cycles` (summed across all ready hits waiting each stalled cycle).
- **ROB occupancy:** `rob_max_occupancy` and `rob_mean_occupancy` sampled at end of each active cycle.

---

## 6. Workload Generators (`include/workloads.h`, `src/workloads.cpp`)

| Workload | Access Pattern | Purpose |
|---|---|---|
| `sequential` | 64 B cache-line linear scans per stream across working set | High spatial locality (64 accesses per 4 KiB page); tests baseline hit-dominated throughput. |
| `strided` | Configurable byte stride (default 4096 B = 1 page/access) | Severe TLB thrashing once working set exceeds DTLB reach. |
| `random` | Uniform random 64 B cache line across working set | Zero spatial/temporal locality; stresses pure walker throughput. |
| `zipf` | Power-law page distribution (`P(k) ∝ 1/(k+1)^α`, default `α = 1.05`) | Realistic hot-set + cold-tail mix; primary testbed for hits waiting behind occasional misses. |
| `pointer_chase` | Cyclic Sattolo permutation across pages with per-stream serial dependency | Models latency-bound linked-list/graph traversal (`1` in-flight pointer per stream). |
| `tiled_gemm` | Blocked `C += A × B` (FP32, tile size 32) mapping `IF` (hot code loop), `Load A` (row-major tile), `Load B` (row-strided panel), `Store C` (accum tile) | ML compute kernel where high-hit-rate `IF` and `A` streams experience HoL blocking behind `B`-panel TLB misses. |
| `kv_cache` | Hot `IF` stream + long 256 B sequential KV-cache sweeps interrupted with probability `0.08` by uniform random paged-KV block lookups | LLM autoregressive decode pattern combining streaming reads with sparse/paged block indirection. |
| `trace` | Replays `src,vaddr` or `cycle,src,vaddr` CSV traces | Enables exact trace cross-validation and direct replay of CPU memory streams. |

---

## 7. Illustrative Default Parameters and Public Citations

The default baseline (`configs/baseline_zipf.json`) uses **illustrative** parameters grounded in public microarchitecture references:

| Parameter | Default | Public Reference & Rationale |
|---|---|---|
| `dtlb.entries = 32`, `assoc = "full"`, `hit_cycles = 1` | 32-entry fully associative L1 DTLB, 1-cycle lookup | Typical L1 DTLB geometry for mid-range / embedded 64-bit cores (e.g., ARM Cortex-A55/A76 L1 DTLB ranges 32–48 entries fully associative, 1-cycle hit; Hennessy & Patterson, *Computer Architecture: A Quantitative Approach*, 6th ed., Ch. 2). |
| `dtlb.page_bytes = 4096` (option: `2097152`) | 4 KiB base page / 2 MiB hugepage | Standard 4 KiB base granule and 2 MiB block/superpage size in both RISC-V Sv39 and ARMv8-A (4 KiB granule). |
| `walker.levels = 3`, `walker.count = 1` | 3-level walk, 1 hardware walker | RISC-V Sv39 uses a 3-level radix page table (RISC-V Privileged Spec, §4.4); ARMv8-A 48-bit VA uses 4 levels (`--levels 4`). A single walker isolates and exposes head-of-line blocking before scaling `count = 2, 4`. |
| `memory.access_cycles = 40` | 40 cycles per PTE read | Illustrative mid-point representing L2/L3-hit to main-memory page-table reads (roughly 12–15 cycles for L2 hit, ~40 cycles for LLC hit, ~150–250 cycles for DRAM miss on modern 2–4 GHz cores). |
| `requester_queue_depth = 8`, `rob_entries = 64`, `mshr_entries = 16` | Bounded issue/reorder buffers | Representative small-to-medium core LSU/ROB/MSHR capacities ensuring finite, apples-to-apples concurrency across policies. |

---

## 8. Explicit Limitations and Non-Goals

To keep the model transparent, fast, and analytically verifiable, the following real-silicon effects are **deliberately not modeled**:

1. **No Page-Walk Caches (PWC / PDE caches):** Every cold miss performs all `L` memory accesses (`L × access_cycles`). Real high-performance MMUs cache upper page-table levels (L2/L1 PDEs), cutting common walks from 3–4 accesses down to 1–2.
2. **No L2 STLB hierarchy:** Only a single-level DTLB is modeled (no shared L2 TLB filtering misses before the walker).
3. **No speculative / prefetched page walks or TLB prefetchers:** Walks start strictly on demand after a confirmed DTLB miss.
4. **No coherence, ASIDs, or TLB shootdowns:** Single address space, no `SFENCE.VMA` / `TLBI` invalidations or multi-core shootdown storms.
5. **Abstract memory timing:** Fixed per-access latency (plus optional BW/queue caps) rather than cycle-accurate cache hierarchy + DRAM bank/row-buffer scheduling.
6. **Transaction-level abstraction vs. RTL:** Does not model pipeline flush/replay on mispredicted branches, physical port arbitration delays, or SRAM banking conflicts.
