`timescale 1ns/10ps

/* IF Stage:
 * - Drives PC, fetches instruction from instruction memory
 * - Computes PC+4
 * - Selects PC+4 or branchAddress depending on brTaken
 */
module InstructionFetch (Instruction, currentPC, branchAddress, brTaken, clk, reset);
	input  logic [63:0] branchAddress;
	input  logic        brTaken, clk, reset;

	output logic [63:0] currentPC;
	output logic [31:0] Instruction;

	logic [63:0] addedPC, nextPC;

	instructmem InstructionMemory (.address(currentPC), .instruction(Instruction), .clk(clk));

	adder_64 thePCAdder (.sum(addedPC), .cout(), .a(currentPC), .b(64'd4), .cin(1'b0));

	mux2_64 BrMUX (.out(nextPC), .i0(addedPC), .i1(branchAddress), .sel(brTaken));

	pc PC (.in(nextPC), .out(currentPC), .reset(reset), .clk(clk));
endmodule
