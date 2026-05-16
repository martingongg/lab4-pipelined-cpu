`timescale 1ns/10ps

/* Decode stage: register file read, immediate generation, branch-target
 * computation, and forwarding mux for read data. Branches resolve here so
 * the IF stage can redirect on the next cycle (1 delay slot).
 */
module InstructionDecode (clk, reset,
                          DecPC, DecInst, DecReg2Loc, DecReg2Write,
                          DecUncondBr, DecBrSrc,
                          WbDataToReg, WbRegWrite, WbAw,
                          DecAa, DecAb, DecAw, DecDa, DecDb,
                          DecImm12Ext, DecImm9Ext,
                          DecBranchPC,
                          ForwardA, ForwardB,
                          ExALUOut, WbMuxOut, DecZero);
	input  logic        clk, reset, DecReg2Loc, DecUncondBr, DecBrSrc;
	input  logic        WbRegWrite, DecReg2Write;
	input  logic [63:0] DecPC, WbDataToReg, ExALUOut, WbMuxOut;
	input  logic [31:0] DecInst;
	input  logic [4:0]  WbAw;
	input  logic [1:0]  ForwardA, ForwardB;

	output logic [63:0] DecDa, DecDb;
	output logic [4:0]  DecAa, DecAb, DecAw;
	output logic [63:0] DecImm9Ext, DecImm12Ext, DecBranchPC;
	output logic        DecZero;

	// First read register is always Inst[9:5] = Rn
	assign DecAa = DecInst[9:5];

	// Reg2Write mux: write-back register address (Rd or X30)
	mux2_5 MuxReg2Write (.out(DecAw),
	                     .i0(DecInst[4:0]), .i1(5'd30),
	                     .sel(DecReg2Write));

	// Reg2Loc mux: second read register address (Rd or Rm)
	mux2_5 MuxReg2Loc (.out(DecAb),
	                   .i0(DecInst[4:0]), .i1(DecInst[20:16]),
	                   .sel(DecReg2Loc));

	// Register file. Write on negedge of clk so the read in the same cycle
	// after a write returns the new value (handled by ~clk).
	logic [63:0] RegDa, RegDb;
	regfile RegisterFile (.ReadData1(RegDa), .ReadData2(RegDb),
	                      .WriteData(WbDataToReg),
	                      .ReadRegister1(DecAa), .ReadRegister2(DecAb),
	                      .WriteRegister(WbAw), .RegWrite(WbRegWrite),
	                      .clk(~clk));

	// Forwarding muxes: pick reg-file data, EX-result, or WB data.
	mux3_64 FwdAMux (.out(DecDa),
	                 .i00(RegDa), .i01(ExALUOut), .i10(WbMuxOut), .i11(64'd0),
	                 .sel(ForwardA));
	mux3_64 FwdBMux (.out(DecDb),
	                 .i00(RegDb), .i01(ExALUOut), .i10(WbMuxOut), .i11(64'd0),
	                 .sel(ForwardB));

	// Zero detect on DecDb (used for CBZ).
	assign DecZero = (DecDb == 64'd0);

	// Immediate generators
	zeroExtend12 ExtendImm12 (.in(DecInst[21:10]), .out(DecImm12Ext));
	signExtend9  ExtendImm9  (.in(DecInst[20:12]), .out(DecImm9Ext));

	// Branch immediate selection (26-bit unconditional vs 19-bit conditional)
	logic [63:0] brAddrExt, condAddrExt, DecImmBranch;
	signExtend26 ExtBr  (.in(DecInst[25:0]),  .out(brAddrExt));
	signExtend19 ExtCnd (.in(DecInst[23:5]),  .out(condAddrExt));

	mux2_64 theUncondMux (.out(DecImmBranch),
	                      .i0(condAddrExt), .i1(brAddrExt),
	                      .sel(DecUncondBr));

	logic [63:0] shiftedAddr, adderResult;
	shift2       ShAddr (.in(DecImmBranch), .out(shiftedAddr));
	adder_64     BrAdd  (.sum(adderResult), .cout(),
	                     .a(DecPC), .b(shiftedAddr), .cin(1'b0));

	// BR uses register-direct branch target; everything else uses PC+imm<<2
	mux2_64 theBrMux (.out(DecBranchPC),
	                  .i0(adderResult), .i1(DecDb),
	                  .sel(DecBrSrc));
endmodule
