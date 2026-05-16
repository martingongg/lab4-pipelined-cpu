`timescale 1ns/10ps

/* MEM/WB pipeline register. Selects between ALU result, memory output, and
 * PC+4 (used for the BL link register), then registers the chosen value
 * for the write-back into the register file.
 */
module MEM_WB_Reg (clk, reset,
                   MemIncrementedPC, MemAw, MemALUOut, MemMem2Reg,
                   MemRegWrite, MemOut,
                   WbAw, WbDataToReg, WbRegWrite, WbMuxOut);
	input  logic        clk, reset;
	input  logic        MemRegWrite;
	input  logic [1:0]  MemMem2Reg;
	input  logic [4:0]  MemAw;
	input  logic [63:0] MemIncrementedPC, MemOut, MemALUOut;

	output logic        WbRegWrite;
	output logic [4:0]  WbAw;
	output logic [63:0] WbDataToReg, WbMuxOut;

	mux3_64 MuxRegWriteBack (.out(WbMuxOut),
	                         .i00(MemALUOut), .i01(MemOut),
	                         .i10(MemIncrementedPC), .i11(64'd0),
	                         .sel(MemMem2Reg));

	pipeline_register #(.N(64)) WbDataReg (.out(WbDataToReg), .in(WbMuxOut), .reset(reset), .clk(clk));
	pipeline_register #(.N(5))  RdReg     (.out(WbAw),        .in(MemAw),    .reset(reset), .clk(clk));

	D_FF RegWriteReg (.q(WbRegWrite), .d(MemRegWrite), .reset, .clk);
endmodule
