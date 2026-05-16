`timescale 1ns/10ps

/* ID/EX pipeline register: latches data + control signals into the EX stage. */
module ID_EX_Reg (clk, reset,
                  DecIncrementedPC, DecALUOp, DecALUSrc, DecMem2Reg,
                  DecRegWrite, DecMemWrite, DecMemRead, DecFlagWrite,
                  DecAw, DecDa, DecDb, DecImm12Ext, DecImm9Ext,

                  ExIncrementedPC, ExALUOp, ExALUSrc, ExMem2Reg,
                  ExRegWrite, ExMemWrite, ExMemRead, ExFlagWrite,
                  ExAw, ExDa, ExDb, ExImm12Ext, ExImm9Ext);

	input  logic        clk, reset;
	input  logic [63:0] DecIncrementedPC, DecDa, DecDb, DecImm9Ext, DecImm12Ext;
	input  logic [4:0]  DecAw;
	input  logic [2:0]  DecALUOp;
	input  logic [1:0]  DecMem2Reg, DecALUSrc;
	input  logic        DecMemWrite, DecMemRead, DecRegWrite, DecFlagWrite;

	output logic [63:0] ExIncrementedPC, ExDa, ExDb, ExImm9Ext, ExImm12Ext;
	output logic [4:0]  ExAw;
	output logic [2:0]  ExALUOp;
	output logic [1:0]  ExMem2Reg, ExALUSrc;
	output logic        ExMemWrite, ExMemRead, ExRegWrite, ExFlagWrite;

	pipeline_register #(.N(64)) PCReg    (.out(ExIncrementedPC), .in(DecIncrementedPC), .reset(reset), .clk(clk));
	pipeline_register #(.N(64)) DaReg    (.out(ExDa),            .in(DecDa),            .reset(reset), .clk(clk));
	pipeline_register #(.N(64)) DbReg    (.out(ExDb),            .in(DecDb),            .reset(reset), .clk(clk));
	pipeline_register #(.N(64)) Imm9Reg  (.out(ExImm9Ext),       .in(DecImm9Ext),       .reset(reset), .clk(clk));
	pipeline_register #(.N(64)) Imm12Reg (.out(ExImm12Ext),      .in(DecImm12Ext),      .reset(reset), .clk(clk));

	pipeline_register #(.N(5)) RdReg     (.out(ExAw),      .in(DecAw),      .reset(reset), .clk(clk));
	pipeline_register #(.N(3)) ALUOpReg  (.out(ExALUOp),   .in(DecALUOp),   .reset(reset), .clk(clk));
	pipeline_register #(.N(2)) ALUSrcReg (.out(ExALUSrc),  .in(DecALUSrc),  .reset(reset), .clk(clk));
	pipeline_register #(.N(2)) M2RReg    (.out(ExMem2Reg), .in(DecMem2Reg), .reset(reset), .clk(clk));

	D_FF MemWriteReg  (.q(ExMemWrite),  .d(DecMemWrite),  .reset, .clk);
	D_FF MemReadReg   (.q(ExMemRead),   .d(DecMemRead),   .reset, .clk);
	D_FF RegWriteReg  (.q(ExRegWrite),  .d(DecRegWrite),  .reset, .clk);
	D_FF FlagWriteReg (.q(ExFlagWrite), .d(DecFlagWrite), .reset, .clk);
endmodule
