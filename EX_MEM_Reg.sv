`timescale 1ns/10ps

/* EX/MEM pipeline register. */
module EX_MEM_Reg (clk, reset,
                   ExIncrementedPC, ExMem2Reg, ExRegWrite, ExMemWrite,
                   ExMemRead, ExAw, ExDb, ExALUOut,
                   MemIncrementedPC, MemMem2Reg, MemRegWrite, MemMemWrite,
                   MemMemRead, MemAw, MemDb, MemALUOut);
	input  logic        clk, reset;
	input  logic [63:0] ExIncrementedPC, ExDb, ExALUOut;
	input  logic [4:0]  ExAw;
	input  logic [1:0]  ExMem2Reg;
	input  logic        ExMemWrite, ExMemRead, ExRegWrite;

	output logic [63:0] MemIncrementedPC, MemDb, MemALUOut;
	output logic [4:0]  MemAw;
	output logic [1:0]  MemMem2Reg;
	output logic        MemMemWrite, MemMemRead, MemRegWrite;

	pipeline_register #(.N(64)) ALUOutReg (.out(MemALUOut),        .in(ExALUOut),        .reset(reset), .clk(clk));
	pipeline_register #(.N(64)) DbReg     (.out(MemDb),            .in(ExDb),            .reset(reset), .clk(clk));
	pipeline_register #(.N(64)) PCReg     (.out(MemIncrementedPC), .in(ExIncrementedPC), .reset(reset), .clk(clk));

	pipeline_register #(.N(5)) RnReg  (.out(MemAw),      .in(ExAw),      .reset(reset), .clk(clk));
	pipeline_register #(.N(2)) M2RReg (.out(MemMem2Reg), .in(ExMem2Reg), .reset(reset), .clk(clk));

	D_FF MemWriteReg (.q(MemMemWrite), .d(ExMemWrite), .reset, .clk);
	D_FF MemReadReg  (.q(MemMemRead),  .d(ExMemRead),  .reset, .clk);
	D_FF RegWriteReg (.q(MemRegWrite), .d(ExRegWrite), .reset, .clk);
endmodule
