## Compile and run all Lab 4 pipeline-module testbenches.

if {[file exists work]} {
    vdel -all -lib work
}
vlib work

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
vlog -sv instructmem.sv
vlog -sv datamem.sv

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
vlog -sv new_modules_tb.sv

foreach tb {
    pipeline_register_tb
    IF_ID_Reg_tb
    ID_EX_Reg_tb
    EX_MEM_Reg_tb
    MEM_WB_Reg_tb
    ForwardingUnit_tb
    ControlSignal_tb
    Execute_tb
    Memory_tb
    InstructionFetch_tb
    InstructionDecode_tb
} {
    vsim -t 1ps -voptargs="+acc" $tb
    run -all
    quit -sim
}
