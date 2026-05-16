`timescale 1ns/10ps

/* MEM stage: data memory access. xfer_size hard-coded to 8 bytes (LDUR/STUR). */
module Memory (clk, reset, address, MemWrite, MemRead, MemWriteData, MemOut);
	input  logic        clk, reset, MemWrite, MemRead;
	input  logic [63:0] address, MemWriteData;
	output logic [63:0] MemOut;

	datamem DataMemory (.address, .write_enable(MemWrite),
	                    .read_enable(MemRead), .write_data(MemWriteData),
	                    .clk, .xfer_size(4'b1000), .read_data(MemOut));
endmodule
