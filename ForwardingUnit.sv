`timescale 1ns/10ps

/* Forwarding unit.
 *
 * In this pipeline, register reads happen in the Decode stage, so we forward
 * back to Decode from later stages. We forward from the EX/MEM register
 * (alive in EX) and from the MEM/WB register (alive in MEM) to the Decode
 * read ports.
 *
 *   ForwardA / ForwardB:
 *     2'b00 -> use register-file read data
 *     2'b01 -> forward EX-stage result (ExALUOut)  [most recent]
 *     2'b10 -> forward MEM-stage write-back data (WbMuxOut)
 *
 * Never forward when the destination is X31 (XZR) — XZR must always read 0.
 */
module ForwardingUnit (DecAa, DecAb, ExAw, MemAw, ExRegWrite, MemRegWrite,
                       ForwardA, ForwardB);
	input  logic [4:0] DecAa, DecAb, ExAw, MemAw;
	input  logic       ExRegWrite, MemRegWrite;
	output logic [1:0] ForwardA, ForwardB;

	always_comb begin
		// --- ForwardA (for source register Aa) ---
		if (ExRegWrite && (ExAw != 5'd31) && (ExAw == DecAa))
			ForwardA = 2'b01;
		else if (MemRegWrite && (MemAw != 5'd31) && (MemAw == DecAa))
			ForwardA = 2'b10;
		else
			ForwardA = 2'b00;

		// --- ForwardB (for source register Ab) ---
		if (ExRegWrite && (ExAw != 5'd31) && (ExAw == DecAb))
			ForwardB = 2'b01;
		else if (MemRegWrite && (MemAw != 5'd31) && (MemAw == DecAb))
			ForwardB = 2'b10;
		else
			ForwardB = 2'b00;
	end
endmodule
