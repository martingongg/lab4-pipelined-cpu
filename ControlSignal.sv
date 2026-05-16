`timescale 1ns/10ps

/* Decode-stage control signal generator.
 * Uses the high 11 bits of the instruction to select the operation, and
 * produces all needed datapath/pipeline control bits.
 *
 * Instruction set covered (Lab 3): ADDI, ADDS, SUBS, B, B.LT, BL, BR, CBZ, LDUR, STUR.
 *
 * Note on signals:
 *   ALUOp[2:0]    - operation passed to ALU (010 add, 011 sub, 000 passB, ...)
 *   ALUSrc[1:0]   - 00=Db, 01=Imm9 (sign-ext), 10=Imm12 (zero-ext)
 *   Mem2Reg[1:0]  - 00=ALUOut, 01=MemOut, 10=PC+4 (BL link)
 *   Reg2Loc       - 0: read second operand from Rd (Inst[4:0])
 *                   1: read second operand from Rm (Inst[20:16])
 *   Reg2Write     - 0: write to Rd, 1: write to X30 (BL)
 *   BrSrc         - 0: branch target = PC + sext(imm)<<2
 *                   1: branch target = Reg[Rd]   (BR)
 *   UncondBr      - 1 for B/BL  (use 26-bit immediate); 0 for B.LT/CBZ (19-bit)
 *   BrTaken       - high when this instruction in Decode is a taken branch
 *   FlagWrite, RegWrite, MemWrite, MemRead - as named.
 */
module ControlSignal (Instruction, ALUOp, ALUSrc, Mem2Reg,
                      BrTaken, BrSrc, Reg2Loc, Reg2Write,
                      RegWrite, MemWrite, MemRead, UncondBr,
                      NegativeFlag, OverflowFlag, ZeroFlag,
                      FlagWrite, BLTBrTaken);

	input  logic [31:0] Instruction;
	input  logic        NegativeFlag, OverflowFlag, ZeroFlag, BLTBrTaken;

	output logic [2:0]  ALUOp;
	output logic [1:0]  ALUSrc, Mem2Reg;
	output logic        BrTaken, BrSrc, Reg2Loc, Reg2Write,
	                    RegWrite, MemWrite, MemRead, UncondBr, FlagWrite;

	enum logic [10:0] {
		B    = 11'b000101xxxxx,
		BLT  = 11'b01010100xxx,
		BL   = 11'b100101xxxxx,
		BR   = 11'b11010110000,
		CBZ  = 11'b10110100xxx,
		ADDI = 11'b1001000100x,
		ADDS = 11'b10101011000,
		SUBS = 11'b11101011000,
		LDUR = 11'b11111000010,
		STUR = 11'b11111000000
	} OpCodes;

	always_comb begin
		// Defaults (all writes off)
		ALUOp     = 3'b000;
		ALUSrc    = 2'b00;
		Mem2Reg   = 2'b00;
		BrTaken   = 1'b0;
		BrSrc     = 1'b0;
		Reg2Loc   = 1'b0;
		Reg2Write = 1'b0;
		RegWrite  = 1'b0;
		MemWrite  = 1'b0;
		MemRead   = 1'b0;
		UncondBr  = 1'b0;
		FlagWrite = 1'b0;

		casex (Instruction[31:21])
			B: begin
				UncondBr = 1'b1; BrTaken = 1'b1;
			end
			BLT: begin
				UncondBr = 1'b0; BrTaken = BLTBrTaken;
			end
			BL: begin
				UncondBr = 1'b1; BrTaken = 1'b1;
				RegWrite = 1'b1; Reg2Write = 1'b1; Mem2Reg = 2'b10;
			end
			BR: begin
				BrTaken = 1'b1; BrSrc = 1'b1;
				Reg2Loc = 1'b0; ALUSrc = 2'b00;
			end
			CBZ: begin
				ALUOp = 3'b000; UncondBr = 1'b0; BrTaken = ZeroFlag;
				Reg2Loc = 1'b0;
			end
			ADDI: begin
				ALUOp = 3'b010; RegWrite = 1'b1; ALUSrc = 2'b10; Mem2Reg = 2'b00;
			end
			ADDS: begin
				ALUOp = 3'b010; RegWrite = 1'b1; ALUSrc = 2'b00;
				Mem2Reg = 2'b00; Reg2Loc = 1'b1; FlagWrite = 1'b1;
			end
			SUBS: begin
				ALUOp = 3'b011; RegWrite = 1'b1; ALUSrc = 2'b00;
				Mem2Reg = 2'b00; Reg2Loc = 1'b1; FlagWrite = 1'b1;
			end
			LDUR: begin
				ALUOp = 3'b010; RegWrite = 1'b1; MemRead = 1'b1;
				ALUSrc = 2'b01; Mem2Reg = 2'b01;
			end
			STUR: begin
				ALUOp = 3'b010; MemWrite = 1'b1;
				ALUSrc = 2'b01; Reg2Loc = 1'b0;
			end
			default: /* defaults already set */ ;
		endcase
	end
endmodule
