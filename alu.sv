`timescale 1ps/1ps
// EE469 Lab 2 - 64-bit ARM ALU
// Andy Wu and Martin Gong
// April 17th 2026
// Chains 64 alu_bit slices and computes 4 status flags
//
// cntrl encoding:
//   000: result = B
//   010: result = A + B
//   011: result = A - B
//   100: result = A & B
//   101: result = A | B
//   110: result = A ^ B

module alu(A, B, cntrl, result, negative, zero, overflow, carry_out);

    input  [63:0] A, B;
    input  [2:0]  cntrl;
    output [63:0] result;
    output negative, zero, overflow, carry_out;

    // carry[0]  = carry into bit 0  = cntrl[0] (0 for add, 1 for subtract gives the +1)
    // carry[i+1]= carry out of bit i, fed into bit i+1
    // carry[64] = final carry out of bit 63
    logic [64:0] carry;
    assign carry[0] = cntrl[0]; //inital state

    // Chain 64 alu_bit slices
    genvar i;
    generate
        for (i = 0; i < 64; i = i+1) begin : alu_chain
            alu_bit ab(
                .result(result[i]),
                .carry_out(carry[i+1]),
                .A(A[i]),
                .B(B[i]),
                .cntrl(cntrl),
                .carry_in(carry[i])
            );
        end
    endgenerate

    //different flag

    // Negative:  2's complement style
    assign negative = result[63];

    // Carry out: final carry out of the adder chain   
    assign carry_out = carry[64];

    // Overflow: the case of 0 1 mismatch 
    xor #(50) ov_gate(overflow, carry[63], carry[64]);

    // Zero: result is zero when ALL 64 bits are 0
    // Built 3-stage OR tree then final NOR checker
    //16 four-input 
    //four-input
    //final check
    logic [15:0] z1;
    logic [3:0]  z2;
    generate
        for (i = 0; i < 16; i = i+1) begin : zero_stage1
            or #(50) orz1(z1[i], result[4*i], result[4*i+1], result[4*i+2], result[4*i+3]);
        end
    endgenerate
    generate
        for (i = 0; i < 4; i = i+1) begin : zero_stage2
            or #(50) orz2(z2[i], z1[4*i], z1[4*i+1], z1[4*i+2], z1[4*i+3]);
        end
    endgenerate
    nor #(50) zero_gate(zero, z2[0], z2[1], z2[2], z2[3]);

endmodule
