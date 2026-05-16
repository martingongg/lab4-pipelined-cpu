## Adds the signals required by the lab spec to the wave window:
##   all 32 architectural registers, PC, flags, data memory, clk, reset.

onerror {resume}
quietly WaveActivateNextPane {} 0

# --- Clock / Reset ---
add wave -divider {Clock & Reset}
add wave -radix unsigned    /cpu_tb/clk
add wave -radix unsigned    /cpu_tb/reset

# --- Program Counter ---
add wave -divider {Program Counter}
add wave -radix hex         /cpu_tb/PC
add wave -radix hex         /cpu_tb/dut/FetchInst
add wave -radix hex         /cpu_tb/dut/DecInst

# --- Flags ---
add wave -divider {Flags  N C V Z}
add wave -radix binary      /cpu_tb/dut/ExNegative
add wave -radix binary      /cpu_tb/dut/ExCarryout
add wave -radix binary      /cpu_tb/dut/ExOverflow
add wave -radix binary      /cpu_tb/dut/theExStage/TheFlagRegister/out

# --- Architectural Register File (X0..X30; X31 = XZR) ---
add wave -divider {Register File}
add wave -radix hex         /cpu_tb/X0
add wave -radix hex         /cpu_tb/X1
add wave -radix hex         /cpu_tb/X2
add wave -radix hex         /cpu_tb/X3
add wave -radix hex         /cpu_tb/X4
add wave -radix hex         /cpu_tb/X5
add wave -radix hex         /cpu_tb/X6
add wave -radix hex         /cpu_tb/X7
add wave -radix hex         /cpu_tb/X8
add wave -radix hex         /cpu_tb/X9
add wave -radix hex         /cpu_tb/X10
add wave -radix hex         /cpu_tb/X11
add wave -radix hex         /cpu_tb/X12
add wave -radix hex         /cpu_tb/X13
add wave -radix hex         /cpu_tb/X14
add wave -radix hex         /cpu_tb/X15
add wave -radix hex         /cpu_tb/X16
add wave -radix hex         /cpu_tb/X17
add wave -radix hex         /cpu_tb/X18
add wave -radix hex         /cpu_tb/X19
add wave -radix hex         /cpu_tb/X20
add wave -radix hex         /cpu_tb/X21
add wave -radix hex         /cpu_tb/X22
add wave -radix hex         /cpu_tb/X23
add wave -radix hex         /cpu_tb/X24
add wave -radix hex         /cpu_tb/X25
add wave -radix hex         /cpu_tb/X26
add wave -radix hex         /cpu_tb/X27
add wave -radix hex         /cpu_tb/X28
add wave -radix hex         /cpu_tb/X29
add wave -radix hex         /cpu_tb/X30

# --- Data memory (first 64 bytes) ---
add wave -divider {Data Memory  (bytes 0..63)}
for {set i 0} {$i < 64} {incr i} {
    add wave -radix hex /cpu_tb/dut/theMemStage/DataMemory/mem($i)
}

TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 1} {0 ps}}
quietly wave cursor active 1
configure wave -namecolwidth 230
configure wave -valuecolwidth 110
configure wave -justifyvalue left
configure wave -signalnamewidth 1
configure wave -snapdistance 10
configure wave -datasetprefix 0
configure wave -rowmargin 4
configure wave -childrowmargin 2
configure wave -gridoffset 0
configure wave -gridperiod 1
configure wave -griddelta 40
configure wave -timeline 0
configure wave -timelineunits ns
update
WaveRestoreZoom {0 ps} {200 us}
