`timescale 1ns/10ps

/* IF/ID pipeline register: latches instruction and PC for the Decode stage. */
module IF_ID_Reg (clk, reset, FetchPC, FetchInst, DecPC, DecInst);
	input  logic        clk, reset;
	input  logic [63:0] FetchPC;
	input  logic [31:0] FetchInst;
	output logic [63:0] DecPC;
	output logic [31:0] DecInst;

	pipeline_register #(.N(32)) InstReg (.out(DecInst), .in(FetchInst), .reset(reset), .clk(clk));
	pipeline_register #(.N(64)) PCReg   (.out(DecPC),   .in(FetchPC),   .reset(reset), .clk(clk));
endmodule
