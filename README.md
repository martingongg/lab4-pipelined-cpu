# EE 469 — Pipelined ARM CPU

A 64-bit, 5-stage pipelined ARM CPU built from the Lab 3 / Lab 4 specifications and the existing single-cycle CPU at <https://github.com/EuntaeKi/EE-469-Projects>.

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
