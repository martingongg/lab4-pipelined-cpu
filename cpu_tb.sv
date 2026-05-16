`timescale 1ns/10ps

/* ============================================================
 * Top-level CPU testbench.
 *
 * Drives the pipelined CPU through the program currently selected in
 * instructmem.sv (`BENCHMARK). All 32 registers, the PC, ALU output,
 * data memory, flags, and clock/reset are exposed via hierarchical
 * references so they can be observed in waveform viewers (modelsim,
 * gtkwave).
 *
 * Long enough clock period (10 us) to satisfy the lab requirement that
 * "all processing is done within this clock cycle". Runs 200 clock
 * cycles - more than enough for every Lab 3 / Lab 4 benchmark.
 * ============================================================ */
module cpu_tb ();
	logic clk, reset;

	CPU dut (.clk, .reset);

	// PC and other observability hooks (purely for waveform viewing).
	wire [63:0] PC      = dut.FetchPC;
	wire [63:0] ALU_OUT = dut.ExALUOut;
	wire [3:0]  FLAGS   = {dut.ExNegative, dut.ExZero, dut.ExOverflow, dut.ExCarryout};

	// Clock generation — long period as the lab calls for.
	parameter CLOCK_PERIOD = 10000;
	initial begin
		clk <= 0;
		forever #(CLOCK_PERIOD/2) clk <= ~clk;
	end

	int i;
	initial begin
		// Reset for two cycles
		reset = 1; @(posedge clk); @(posedge clk);
		reset = 0; @(posedge clk);

		// Run plenty of cycles to drain the longest benchmark (sort/Fib).
		for (i = 0; i < 200; i++) begin
			@(posedge clk);
		end

		$display("--- Final CPU state ---");
		$display("PC=%h ALU_OUT=%h FLAGS=%b", PC, ALU_OUT, FLAGS);
		$stop;
	end
endmodule
