# Validation Methodology and Results

Credible performance models must be verified against closed-form analytical bounds, structural invariants, an independent reference implementation, and deterministic regressions. Every check documented below is automated and executed by `ctest --test-dir build --output-on-failure`.

---

## 1. Analytical Latency Formulas (`tests/test_perf_model.cpp`)

Let $H = \text{dtlb.hit\_cycles}$, $L = \text{walker.levels}$, and $M = \text{memory.access\_cycles}$.

### 1.1 Uncontended DTLB Hit
For a single requester (`requester_queue_depth = 1`) issuing into a warmed DTLB (100% hit rate):
- Transaction issues at cycle $t_{\text{issue}} = c$.
- Arbiter grants at cycle $t_{\text{lookup}} = c$.
- DTLB lookup resolves as a hit at cycle $t_{\text{done}} = c + H$.
- Retire stage retires immediately in the same cycle $t_{\text{retire}} = c + H$.

$$\text{Latency}_{\text{hit}} = t_{\text{done}} - t_{\text{issue}} = t_{\text{retire}} - t_{\text{issue}} = H$$

- **Verified by:** `Analytical.AllHitsExactHitCycles` for $H \in \{1, 3\}$ across 500 transactions (`min == max == mean == H`, `dtlb_misses == 0`, `hol_stall_cycles == 0`).

### 1.2 Single Cold Miss
For an isolated cold miss (no queueing, unlimited memory bandwidth):
- Transaction issues and enters DTLB at cycle $c$.
- DTLB resolves as a miss at cycle $c + H$ and immediately claims a free walker to issue page-table Level 1 memory read.
- Level $k \in \{1, \dots, L\}$ completes at cycle $c + H + k \cdot M$ and immediately launches Level $k + 1$ (zero inter-level bubble).
- At cycle $c + H + L \cdot M$, the final PTE returns, the VPN installs into the DTLB, and the transaction completes and retires.

$$\text{Latency}_{\text{cold\_miss}} = H + L \times M$$

- **Verified by:** `Analytical.SingleColdMissExactFormula` across all three policies (`InOrderFifo`, `RoundRobin`, `IdAwareBypass`) for:
  - $(H=1, L=3, M=40) \implies \mathbf{121}$ cycles (Sv39-style default)
  - $(H=2, L=4, M=25) \implies \mathbf{102}$ cycles (ARMv8 4-level style)
  - $(H=1, L=2, M=10) \implies \mathbf{21}$ cycles

### 1.3 Memory Bandwidth Queueing Check
With `memory.access_cycles = 10` and `max_requests_per_cycle = 1`, two requests submitted on cycle `0` must complete on cycles `10` and `11` respectively (`MemoryUnit.BandwidthCapAddsDeterministicQueueingDelay`).

---

## 2. Structural Invariants (`tests/test_perf_model.cpp`)

| Invariant | Test Name | Verification Scope |
|---|---|---|
| Single completion / accounting conservation | `Invariants.SingleCompletionAndInOrderRetireAllPolicies` | Every transaction completes and retires exactly once (`transactions_completed == N`, `dtlb_hits + dtlb_misses == N`) across all 3 policies. |
| Strict in-order retirement by ID | `Invariants.SingleCompletionAndInOrderRetireAllPolicies` | With `retire_in_order = true`, the recorded retirement ID vector equals `[0, 1, 2, ..., N-1]` verbatim across all 3 policies. |
| Starvation freedom under `RoundRobin` | `Invariants.RoundRobinPreventsStarvation` | Replays an asymmetric burst trace (120 requests from `src=0` followed by 1 from `src=1` at cycle 0); confirms `RoundRobin` strictly bounds `src=1` latency well below `InOrderFifo`. |
| Byte-for-byte determinism | `Invariants.DeterministicRepeatableOutput` | Two independent runs with identical seed produce byte-identical JSON output. |
| True-LRU & set-associative indexing | `DtlbUnit.FullAndSetAssociativeLruEviction` | Confirms exact per-set LRU victim selection on a 2-way, 4-entry DTLB. |
| 2 MiB hugepage reach | `DtlbUnit.HugePage2MiBReachReducesMisses` | Confirms a 32 MiB KV-cache footprint (8,192 × 4 KiB pages vs. 16 × 2 MiB pages) yields higher hit rate and lower latency under 2 MiB pages. |
| All workloads terminate cleanly | `Workloads.AllSevenBuiltInWorkloadsCompleteCleanly` | Verifies `sequential`, `strided`, `random`, `zipf`, `pointer_chase`, `tiled_gemm`, and `kv_cache`. |

---

## 3. Independent Python LRU Reference (`scripts/ref_tlb.py`)

`scripts/ref_tlb.py` implements an independent true-LRU TLB from scratch using Python's `collections.OrderedDict` (zero shared code with C++). Registered in CTest as `python_lru_reference`, it runs **11 verification cases** (4,000 transactions each) in two modes:

1. **Pure Trace Replay (7 cases — `InOrderFifo`, 1 walker):** `translation_sim --dump-trace` exports every `(id, src, vaddr, tlb_hit)`. Because single-walker `InOrderFifo` completes each walk and installs its VPN before the next lookup executes, the Python reference replays the raw virtual address stream directly and asserts **exact per-transaction and aggregate hit/miss equality** across:
   - `zipf` (32-entry full-assoc, 4 KiB), `zipf` (32-entry 4-way, 4 KiB)
   - `strided` (16-entry direct-mapped, 4 KiB)
   - `random` (32-entry full-assoc, 2 MiB hugepages)
   - `tiled_gemm` (32-entry full-assoc, 4 KiB)
   - `kv_cache` (32-entry 4-way, 4 KiB)
   - `pointer_chase` (32-entry full-assoc, 4 KiB)
2. **Chronological Event-Log Replay (4 cases — `RoundRobin` and `IdAwareBypass`, 1–2 walkers):** Because hit-under-miss and multi-walker pipelines interleave lookups with delayed walk fills, `translation_sim --dump-tlb-events` exports the chronological stream of `LOOKUP` and `INSTALL` operations. The Python reference replays every event and asserts **every individual lookup's hit/miss bit matches C++ identically**.

---

## 4. Deterministic Golden-File Regression (`scripts/regress.py`)

Five fixed-seed configurations (`configs/golden_*.json`, 20,000 transactions each) covering `InOrderFifo`, `RoundRobin`, `IdAwareBypass`, `tiled_gemm`, and `kv_cache` have checked-in reference outputs in `results/golden/`.

Registered in CTest as `golden_regression`, `scripts/regress.py` reruns all five configs and checks for **exact recursive JSON equality**, exiting nonzero with a field-level diff on any discrepancy.

To intentionally refresh golden baselines after a deliberate model change:
```bash
python3 scripts/regress.py --binary build/translation_sim --configs-dir configs --golden-dir results/golden --update
```

---

## 5. Sanity Trends (`tests/test_perf_model.cpp` & Full Sweep)

All expected qualitative microarchitectural trends are enforced by unit tests and confirmed across the 156-point sweep:

1. **Working set exceeding DTLB reach (`SanityTrends.LatencyRisesAboveDtlbReach`):**
   - At `working_set_pages <= 32` (within the 32-entry DTLB reach), latency stays near baseline (~32–34 cycles across 4 concurrent requesters).
   - Beyond 32 pages (`64 → 1024`), miss rate climbs sharply and mean latency rises monotonically across Zipfian, Tiled GEMM, and KV-Cache (`docs/figures/latency_vs_working_set.png`).
2. **Multi-walker scaling (`SanityTrends.MoreWalkersReducesHolStallsUnderInOrderFifo`):**
   - On 256-page Zipf (100k txns, `InOrderFifo`), scaling `walker.count` from `1 → 2 → 4` reduces HoL stall cycles monotonically from **5,123,400 → 2,488,079 (-51.4%) → 1,167,115 (-77.2%)** and scales throughput from **0.0191 → 0.0386 → 0.0789 txns/cycle**.
3. **Hit-under-miss tradeoff (`SanityTrends.IdAwareBypassReducesHolBlockingAtRobCost`):**
   - On 256-page Zipf, `IdAwareBypass` cuts HoL stall cycles by **96.1%** and mean completion latency by **62.1%**, while increasing max ROB occupancy from **1** (`InOrderFifo`) to **32** entries.
4. **Little's Law consistency check:**
   - Under closed-loop steady-state concurrency ($L \approx 32$ outstanding transactions across 4 requesters × depth 8), measured mean retirement latency matches $L / \lambda$ closely across workloads (e.g., on 256-page Zipf `InOrderFifo`: $32 / 0.019143 = 1671.6$ cycles vs. measured $1671.3$ cycles; `IdAwareBypass`: $32 / 0.021976 = 1456.1$ cycles vs. measured $1455.7$ cycles).
