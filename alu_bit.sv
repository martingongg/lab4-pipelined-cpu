`timescale 1ps/1ps

// 1-bit ALU slice
// cntrl encoding:
//   000: result = B
//   010: result = A + B
//   011: result = A - B
//   100: result = A & B
//   101: result = A | B
//   110: result = A ^ B

module alu_bit(result, carry_out, A, B, cntrl, carry_in);

    output result, carry_out;
    input  A, B;
    input  [2:0] cntrl;
    input  carry_in;

    // XOR B with cntrl[0] to optionally invert B
    // cntrl[0]=1 for subtract (011) AND or (101)
    wire b_inv;
    xor #(50) bx(b_inv, B, cntrl[0]);

    // Full adder: carry_out feeds the next bit's carry_in
    wire sum;
    full_adder fa(.sum(sum), .cout(carry_out), .a(A), .b(b_inv), .cin(carry_in));

    // Bitwise logic always computed, mux selects which one is used
    wire and_out, or_out, xor_out;
    and #(50) ag(and_out, A, B);
    or  #(50) og(or_out,  A, B);
    xor #(50) xg(xor_out, A, B);

    // Use a 8:1 mux to pick the final result based on cntrl
    // {B, xor_out, or_out, and_out, sum, sum, B, B}
    // in[7]  in[6]   in[5]  in[4]  in[3] in[2] in[1] in[0]

    mux8_1 rm(
        .out(result),
        .in({B, xor_out, or_out, and_out, sum, sum, B, B}),
        .sel(cntrl)
    );

endmodule
