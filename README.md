# EE 469 — Pipelined ARM CPU & Translation-Path Performance Model

A 64-bit, 5-stage pipelined ARM LEGv8 CPU in SystemVerilog paired with a cycle-driven **C++17 virtual-memory translation-path performance model** (`perf-model/`).

| Layer | Location | Language / Tools | Scope |
|---|---|---|---|
| **Core RTL** | Repo root (`*.sv`, `*.do`) | SystemVerilog, ModelSim/Questa | 64-bit 5-stage pipelined integer core (`IF → ID → EX → MEM → WB`), EX/MEM forwarding, branch delay slots, exhaustive primitive testbenches, ideal single-cycle physical memories (`instructmem.sv`, `datamem.sv`). |
| **Translation-Path Performance Model** | [`perf-model/`](perf-model/) | C++17, CMake, GoogleTest, Python (`numpy`, `pandas`, `matplotlib`) | Cycle-driven transaction-level architectural model of a DTLB, multi-level hardware page-table walker, request arbiter (`InOrderFifo`, `RoundRobin`, `IdAwareBypass`), memory latency model, and reorder/retire stage servicing `IF`, `Load`, `Store`, and `Prefetch` streams. |

---

## Part 1 — 5-Stage Pipelined ARM CPU (SystemVerilog RTL)

Built from the Lab 3 / Lab 4 specifications and the existing single-cycle CPU at <https://github.com/EuntaeKi/EE-469-Projects>.

Branches resolve in the **Decode** stage (one delay slot per branch / load), and EX→ID and MEM→ID forwarding paths are provided so that back-to-back data dependencies execute correctly without stalls (assuming the load-use delay slot is honored).

## Pipeline overview

```
┌────┐  ┌─────┐  ┌────┐  ┌─────┐  ┌─────┐  ┌────┐  ┌─────┐  ┌────┐  ┌─────┐
│ IF │─▶│IF/ID│─▶│ ID │─▶│ID/EX│─▶│ EX  │─▶│EX/M│─▶│ MEM │─▶│M/WB│─▶│ WB  │
└────┘  └─────┘  └────┘  └─────┘  └─────┘  └────┘  └─────┘  └────┘  └─────┘
                    ▲                          │           │
                    └────── ForwardA/B ────────┴───────────┘
```

## Instruction set

| Mnemonic | Operation                                                 |
|----------|-----------------------------------------------------------|
| ADDI     | `Reg[Rd] = Reg[Rn] + ZeroExtend(Imm12)`                   |
| ADDS     | `Reg[Rd] = Reg[Rn] + Reg[Rm]`, sets flags                 |
| SUBS     | `Reg[Rd] = Reg[Rn] - Reg[Rm]`, sets flags                 |
| B        | `PC = PC + SignExtend(Imm26<<2)` (delay slot)             |
| B.LT     | If `N≠V`, `PC = PC + SignExtend(Imm19<<2)` (delay slot)   |
| BL       | `X30 = PC + 4`, `PC = PC + SignExtend(Imm26<<2)` (delay)  |
| BR       | `PC = Reg[Rd]` (delay slot)                                |
| CBZ      | If `Reg[Rd] == 0`, `PC = PC + SignExtend(Imm19<<2)` (delay)|
| LDUR     | `Reg[Rd] = Mem[Reg[Rn] + SignExtend(Imm9)]`               |
| STUR     | `Mem[Reg[Rn] + SignExtend(Imm9)] = Reg[Rd]`               |

X31 (XZR) is hard-wired to 0 and is excluded from the forwarding network.

## File map

### Pipeline stages
- `InstructionFetch.sv` — PC, IM, PC+4 adder, PC mux
- `IF_ID_Reg.sv` — IF→ID pipeline latch
- `InstructionDecode.sv` — register file read, immediate generators, branch-target adder, forwarding muxes
- `ControlSignal.sv` — instruction decode → control bits
- `ForwardingUnit.sv` — picks register-file vs EX vs WB data
- `ID_EX_Reg.sv` — ID→EX pipeline latch
- `Execute.sv` — ALU + flag register + B.LT decision logic
- `EX_MEM_Reg.sv` — EX→MEM pipeline latch
- `Memory.sv` — data-memory wrapper
- `MEM_WB_Reg.sv` — MEM→WB pipeline latch + writeback mux

### Datapath primitives
- `alu.sv` — 64-bit gate-level ALU (built from `alu_1bit` + slices)
- `fullAdder.sv` — 1-bit and 64-bit ripple-carry adders
- `muxes.sv` — gate-level mux2/4/8/16/32 (1-bit and 64-bit variants)
- `decoders.sv` — 2:4, 3:8, and 5:32 decoders
- `registers.sv` — generic N-bit registers, 64-bit register, register file, sign-extender, 64-bit NOR (zero detect)
- `D_FF.sv` — single D flip-flop with synchronous reset
- `FlagReg.sv` — 4-bit flag register {N,C,V,Z}
- `ProgramCounter.sv` — 64-bit PC
- `shifter.sv` — combinational shifter for branch immediates
- `instructmem.sv` / `datamem.sv` — instruction/data memories (provided by the lab)

### Top level + simulation
- `CPU.sv` — pipelined CPU top level
- `cpu_tb.sv` — top-level testbench (drives clock/reset, exposes all 32 registers, PC, flags, and data memory for the wave window)
- `runlab.do` — ModelSim/Questa compile + run script
- `cpu_wave.do` — wave window setup (all registers, PC, flags, data memory, clk, reset)
- `benchmarks/test*.arm` — provided test programs

## Improved testbenches

Every datapath primitive comes with a self-checking testbench that **enumerates every binary input combination** wherever feasible:

| Module          | Inputs covered                  | Cases   |
|-----------------|---------------------------------|---------|
| `D_FF`          | `{reset, d}`                    | 4       |
| `mux2to1`       | `{select, in[1:0]}`             | 8       |
| `mux4to1`       | `{select[1:0], in[3:0]}`        | 64      |
| `mux8to1`       | `{select[2:0], in[7:0]}`        | 2048    |
| `mux2to1_Nbit` (N=4) | `{en, a, b}`               | 512     |
| `decoder2_4`    | `{en, in[1:0]}`                 | 8       |
| `decoder3_8`    | `{en, in[2:0]}`                 | 16      |
| `decoder5_32`   | `{en, in[4:0]}`                 | 64      |
| `fullAdder`     | `{A, B, cin}`                   | 8       |
| `fullAdder_64`  | corner-cases + 200 randoms      | 205     |
| `alu_1bit`      | 6 valid ops × `{A, B, cin}`     | 48      |
| `alu` (64-bit)  | 6 ops × 64 corner pairs + 400 randoms | 784 |
| `registerN` (N=4) | every input value + reset     | 17      |
| `SignExtend` (N=4)| every input value             | 16      |

All testbenches use `assert` to flag mismatches and report `OK` when complete. The 64-bit `alu` cannot be exhaustively swept (2^131 cases is infeasible) — instead it is exercised over an exhaustive corner-pair grid plus randomized inputs.

## Running in ModelSim

```tcl
vsim -do runlab.do
```

The script compiles every file, opens the wave window populated by `cpu_wave.do`, and runs 2 ms of simulated time — long enough for every benchmark to finish.

## Switching benchmarks

Edit `instructmem.sv` and uncomment the desired `\`define BENCHMARK` line. The repo includes:

- `test01_AddiB.arm` — ADDI / B
- `test02_AddsSubs.arm` — ADDS / SUBS, flag tests
- `test03_CbzB.arm` — CBZ / B
- `test04_LdurStur.arm` — load/store
- `test05_Blt.arm` — B.LT
- `test06_BlBr.arm` — BL / BR (function call/return)
- `test10_forwarding.arm` — exercises ALL forwarding paths
- `test11_Sort.arm` — bubble sort
- `test12_Fibonacci.arm` — Fibonacci sequence

---

## Part 2 — Virtual-Memory Translation-Path Performance Model (`perf-model/`)

The core RTL in `CPU.sv` assumes ideal single-cycle physical instruction and data memories (`instructmem.sv`, `datamem.sv`). To analyze how virtual-memory address translation affects fetch and load/store latency on a real processor, [`perf-model/`](perf-model/) adds a self-contained **C++17 cycle-driven performance model** of a DTLB, multi-level page-table walker, request arbiter, memory latency model, and reorder/retire stage.

### Quickstart

```bash
cmake -S perf-model -B perf-model/build -DCMAKE_BUILD_TYPE=Release
cmake --build perf-model/build -j
ctest --test-dir perf-model/build --output-on-failure
```

### Key Highlights (Measured)

- **Three arbitration policies:** `InOrderFifo`, `RoundRobin`, and `IdAwareBypass` (hit-under-miss with MSHR coalescing and in-order ID retirement via ROB).
- **Seven synthetic & ML-shaped workloads + CSV trace replay:** `sequential`, `strided`, `random`, `zipf`, `pointer_chase`, `tiled_gemm` (blocked `C += A × B`), and `kv_cache` (LLM decode streaming + random paged-block indirection).
- **Headline HoL-blocking findings** (32-entry full-assoc DTLB, 1-cycle hit, 3-level walk, 40-cycle memory, 1 walker, 256-page working set = 8× DTLB reach, 100k txns, in-order retirement):
  - **Zipfian:** `IdAwareBypass` reduces mean completion latency by **62.1%** (`1,671.3 → 633.9` cycles), median (`p50`) completion latency from **1,712 → 5 cycles**, and HoL stall cycles by **96.1%** (`98.1% → 4.4%` of runtime), while increasing throughput by **+14.8%** at a cost of **32×** higher max ROB occupancy (`1 → 32`).
  - **KV-Cache:** `IdAwareBypass` reduces mean completion latency by **69.1%** (`402.5 → 124.5` cycles; `-88.7%` for the `IF` stream alone) and eliminates HoL stall cycles (`92.0% → 0.0%`).
  - **2 MiB hugepages (32 MiB footprint):** Raises `tiled_gemm` and `kv_cache` DTLB hit rates to **99.97%** and **99.95%**, cutting mean completion latency to **~31.3 cycles** (`-94.4%` vs. 4 KiB `InOrderFifo` on `tiled_gemm`).
- **Automated verification (17 CTest checks):** 15 GoogleTest unit/analytical/invariant/trend tests, an independent Python LRU reference (`perf-model/scripts/ref_tlb.py`) matching C++ hit/miss counts exactly across 11 trace/event suites, and 5 deterministic golden-file regressions (`perf-model/scripts/regress.py`).

For full architecture diagrams, complete sweep tables, plots, and modeling limitations, see:
- [`perf-model/README.md`](perf-model/README.md) — Full results, figures, and usage
- [`perf-model/docs/design.md`](perf-model/docs/design.md) — Microarchitecture specification, policy definitions, defaults, and limitations
- [`perf-model/docs/validation.md`](perf-model/docs/validation.md) — Analytical derivations, invariants, Python reference, and regression setup

