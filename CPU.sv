`timescale 1ns/10ps

/* ARM 5-stage pipelined CPU (IF / ID / EX / MEM / WB).
 *
 * Supports: ADDI, ADDS, SUBS, B, B.LT, BL, BR, CBZ, LDUR, STUR
 *
 * Branches resolve in the Decode stage, so each branch / load has 1 delay
 * slot (the lab 4 spec assumes the assembler/programmer schedules a useful
 * instruction or NOP immediately after).
 *
 * Forwarding back to Decode covers:
 *   - EX -> ID  (1-instruction-old result)
 *   - MEM -> ID (2-instruction-old result, via the WB mux output)
 *
 * X31 (XZR) is hardwired to 0 in the register file and is excluded from
 * forwarding.
 */
module CPU (clk, reset);
	input  logic clk, reset;

	/*======================================================
	 * IF: Instruction Fetch
	 *======================================================*/
	logic [63:0] FetchPC, DecBranchPC;
	logic [31:0] FetchInst;
	logic        DecBrTaken;

	InstructionFetch theFetchStage (.Instruction(FetchInst),
	                                .currentPC(FetchPC),
	                                .branchAddress(DecBranchPC),
	                                .brTaken(DecBrTaken),
	                                .clk, .reset);

	/*======================================================
	 * IF/ID register
	 *======================================================*/
	logic [63:0] DecPC;
	logic [31:0] DecInst;

	IF_ID_Reg theInstReg (.clk, .reset,
	                      .FetchPC, .FetchInst,
	                      .DecPC,   .DecInst);

	/*======================================================
	 * Control signals
	 *======================================================*/
	logic [2:0]  DecALUOp;
	logic [1:0]  DecALUSrc, DecMem2Reg;
	logic        DecBrSrc, DecReg2Loc, DecReg2Write, DecRegWrite;
	logic        DecMemWrite, DecMemRead, DecUncondBr, DecFlagWrite;
	logic        DecZero, BLTBrTaken, ExOverflow, ExNegative;

	ControlSignal theControlSignals (.Instruction(DecInst),
	                                 .ALUOp(DecALUOp), .ALUSrc(DecALUSrc),
	                                 .Mem2Reg(DecMem2Reg),
	                                 .BrTaken(DecBrTaken),
	                                 .BrSrc(DecBrSrc),
	                                 .Reg2Loc(DecReg2Loc),
	                                 .Reg2Write(DecReg2Write),
	                                 .RegWrite(DecRegWrite),
	                                 .MemWrite(DecMemWrite),
	                                 .MemRead(DecMemRead),
	                                 .UncondBr(DecUncondBr),
	                                 .NegativeFlag(ExNegative),
	                                 .OverflowFlag(ExOverflow),
	                                 .ZeroFlag(DecZero),
	                                 .FlagWrite(DecFlagWrite),
	                                 .BLTBrTaken);

	/*======================================================
	 * Forwarding unit
	 *======================================================*/
	logic [4:0] DecAa, DecAb, ExAw, MemAw;
	logic [1:0] ForwardA, ForwardB;
	logic       ExRegWrite, MemRegWrite, WbRegWrite;

	ForwardingUnit theFwdUnit (.DecAa, .DecAb, .ExAw, .MemAw,
	                           .ExRegWrite, .MemRegWrite,
	                           .ForwardA, .ForwardB);

	/*======================================================
	 * ID: Instruction Decode
	 *======================================================*/
	logic [63:0] DecDa, DecDb, DecImm12Ext, DecImm9Ext;
	logic [4:0]  DecAw, WbAw;
	logic [63:0] WbDataToReg, WbMuxOut, ExALUOut;

	InstructionDecode theDecStage (.clk, .reset,
	                               .DecPC, .DecInst,
	                               .DecReg2Loc, .DecReg2Write,
	                               .DecUncondBr, .DecBrSrc,
	                               .WbDataToReg, .WbRegWrite, .WbAw,
	                               .DecAa, .DecAb, .DecAw,
	                               .DecDa, .DecDb,
	                               .DecImm12Ext, .DecImm9Ext,
	                               .DecBranchPC,
	                               .ForwardA, .ForwardB,
	                               .ExALUOut, .WbMuxOut, .DecZero);

	/*======================================================
	 * ID/EX register
	 *======================================================*/
	logic [63:0] ExDa, ExDb, ExImm12Ext, ExImm9Ext, ExIncrementedPC;
	logic [2:0]  ExALUOp;
	logic [1:0]  ExALUSrc, ExMem2Reg;
	logic        ExMemWrite, ExMemRead, ExFlagWrite;

	ID_EX_Reg theDecReg (.clk, .reset,
	                     .DecIncrementedPC(FetchPC), // PC+4 captured at fetch is FetchPC of next cycle
	                     .DecALUOp, .DecALUSrc, .DecMem2Reg,
	                     .DecRegWrite, .DecMemWrite, .DecMemRead, .DecFlagWrite,
	                     .DecAw, .DecDa, .DecDb,
	                     .DecImm12Ext, .DecImm9Ext,

	                     .ExIncrementedPC, .ExALUOp, .ExALUSrc, .ExMem2Reg,
	                     .ExRegWrite, .ExMemWrite, .ExMemRead, .ExFlagWrite,
	                     .ExAw, .ExDa, .ExDb,
	                     .ExImm12Ext, .ExImm9Ext);

	/*======================================================
	 * EX: Execute
	 *======================================================*/
	logic ExZero, ExCarryout;

	Execute theExStage (.clk, .reset,
	                    .ExDa, .ExDb, .ExALUSrc, .ExALUOp, .ExFlagWrite,
	                    .ExImm12Ext, .ExImm9Ext,
	                    .ExALUOut, .ExOverflow, .ExNegative,
	                    .ExZero, .ExCarryout, .BLTBrTaken);

	/*======================================================
	 * EX/MEM register
	 *======================================================*/
	logic [63:0] MemIncrementedPC, MemDb, MemALUOut;
	logic [1:0]  MemMem2Reg;
	logic        MemMemWrite, MemMemRead;

	EX_MEM_Reg theExReg (.clk, .reset,
	                     .ExIncrementedPC, .ExMem2Reg,
	                     .ExRegWrite, .ExMemWrite, .ExMemRead,
	                     .ExAw, .ExDb, .ExALUOut,

	                     .MemIncrementedPC, .MemMem2Reg,
	                     .MemRegWrite, .MemMemWrite, .MemMemRead,
	                     .MemAw, .MemDb, .MemALUOut);

	/*======================================================
	 * MEM: Memory access
	 *======================================================*/
	logic [63:0] MemOut;

	Memory theMemStage (.clk, .reset,
	                    .address(MemALUOut),
	                    .MemWrite(MemMemWrite), .MemRead(MemMemRead),
	                    .MemWriteData(MemDb), .MemOut);

	/*======================================================
	 * MEM/WB register + write-back
	 *======================================================*/
	MEM_WB_Reg theMemReg (.clk, .reset,
	                      .MemIncrementedPC, .MemAw, .MemALUOut,
	                      .MemMem2Reg, .MemRegWrite, .MemOut,
	                      .WbAw, .WbDataToReg, .WbRegWrite, .WbMuxOut);
endmodule
