`timescale 1ps/1ps

// 32:1 Multiplexer: selects one of thirty-two inputs based on a 5-bit select signal
// Built from two mux16_1 instances and one mux2_1 instance arranged in a two-level tree
module mux32_1 (out, in, sel);
    output logic out;
    input logic [31:0] in;
    input logic [4:0] sel;

    // Intermediate wires carry outputs of the first-level muxes
    wire mu0, mu1;

    // Level 1: sel[3:0] selects among the lower and upper 16-bit halves separately
    mux16_1 m0(.out(mu0), .in(in[15:0]),  .sel(sel[3:0]));
    mux16_1 m1(.out(mu1), .in(in[31:16]), .sel(sel[3:0]));

    // Level 2: sel[4] selects between the two level-1 results
    mux2_1 m2(.out(out), .i0(mu0), .i1(mu1), .sel(sel[4]));
endmodule