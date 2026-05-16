## Compile and run script for ModelSim / Questa
##  Usage:  vsim -do runlab.do

# Create / use working library
if {[file exists work]} {
    vdel -all -lib work
}
vlib work

# --- Compile Lab 3 shared modules first ---
vlog -sv D_FF.sv
vlog -sv mux2_1.sv
vlog -sv mux2_5.sv
vlog -sv mux2_64.sv
vlog -sv mux3_64.sv
vlog -sv mux32_1.sv
vlog -sv mux64_32.sv
vlog -sv decoder_2_4.sv
vlog -sv decoder_3_8.sv
vlog -sv decoder_4_16.sv
vlog -sv decoder_5_32.sv
vlog -sv full_adder.sv
vlog -sv adder_64.sv
vlog -sv alu_bit.sv
vlog -sv alu.sv
vlog -sv pc.sv
vlog -sv regfile.sv
vlog -sv zeroExtend12.sv
vlog -sv signExtend9.sv
vlog -sv signExtend19.sv
vlog -sv signExtend26.sv
vlog -sv shift2.sv
vlog -sv flag_register.sv
vlog -sv pipeline_register.sv

# --- Compile memories ---
vlog -sv instructmem.sv
vlog -sv datamem.sv

# --- Compile pipeline stages ---
vlog -sv InstructionFetch.sv
vlog -sv IF_ID_Reg.sv
vlog -sv ControlSignal.sv
vlog -sv ForwardingUnit.sv
vlog -sv InstructionDecode.sv
vlog -sv ID_EX_Reg.sv
vlog -sv Execute.sv
vlog -sv EX_MEM_Reg.sv
vlog -sv Memory.sv
vlog -sv MEM_WB_Reg.sv

# --- Top level + testbench ---
vlog -sv CPU.sv
vlog -sv cpu_tb.sv

# Start the simulation
vsim -t 10ps -voptargs="+acc" cpu_tb

# Load wave configuration if it exists
if {[file exists cpu_wave.do]} {
    do cpu_wave.do
}

# Run a long enough time to drain the longest benchmark
run 2 ms
