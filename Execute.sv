`timescale 1ns/10ps

/* Execute stage: ALU + flag register + B.LT decision logic.
 *
 * BLTBrTaken is forwarded back to Decode so a B.LT immediately following
 * a flag-setting instruction sees the not-yet-committed flag.
 *   BLTBrTaken = (~ExFlagWrite & (ExNeg ^ ExOver)) | (ExFlagWrite & (Neg ^ Over))
 */
module Execute (clk, reset,
                ExDa, ExDb, ExALUSrc, ExALUOp, ExFlagWrite,
                ExImm12Ext, ExImm9Ext,
                ExALUOut, ExOverflow, ExNegative, ExZero, ExCarryout,
                BLTBrTaken);

	input  logic [63:0] ExDa, ExDb, ExImm12Ext, ExImm9Ext;
	input  logic [2:0]  ExALUOp;
	input  logic [1:0]  ExALUSrc;
	input  logic        ExFlagWrite, clk, reset;

	output logic [63:0] ExALUOut;
	output logic        ExOverflow, ExNegative, ExZero, ExCarryout, BLTBrTaken;

	logic        Overflow, Negative, Zero, Carryout;

	logic [63:0] ALUSrcOut;
	mux3_64 ALUSrcMux (.out(ALUSrcOut),
	                   .i00(ExDb), .i01(ExImm9Ext), .i10(ExImm12Ext), .i11(64'd0),
	                   .sel(ExALUSrc));

	alu TheAlu (.A(ExDa), .B(ALUSrcOut), .cntrl(ExALUOp),
	            .result(ExALUOut),
	            .negative(Negative), .zero(Zero),
	            .overflow(Overflow), .carry_out(Carryout));

	flag_register TheFlagRegister (.clk(clk), .reset(reset), .en(ExFlagWrite),
	                               .alu_negative(Negative),
	                               .alu_zero(Zero),
	                               .alu_overflow(Overflow),
	                               .alu_carry_out(Carryout),
	                               .negative(ExNegative),
	                               .zero(ExZero),
	                               .overflow(ExOverflow),
	                               .carry_out(ExCarryout));

	logic NotExFlagWrite, XorExNegOver, XorNegOver;
	logic AndFlagWriteXor, AndNotFlagWriteXor;
	not #0.05 n0 (NotExFlagWrite, ExFlagWrite);
	xor #0.05 x0 (XorExNegOver, ExNegative, ExOverflow);
	xor #0.05 x1 (XorNegOver,   Negative,   Overflow);
	and #0.05 a0 (AndFlagWriteXor,    ExFlagWrite,    XorNegOver);
	and #0.05 a1 (AndNotFlagWriteXor, NotExFlagWrite, XorExNegOver);
	or  #0.05 o0 (BLTBrTaken,         AndFlagWriteXor, AndNotFlagWriteXor);
endmodule
