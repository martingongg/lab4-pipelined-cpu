`timescale 1ps/1ps

// 64-bit ripple carry adder
// Chains 64 full_adder instances, passing carry-out of bit N into carry-in of bit N+1

module adder_64(sum, cout, a, b, cin);

    output [63:0] sum;  // 64-bit result
    output cout;        // carry-out
    input  [63:0] a, b; // 64-bit operands
    input  cin;         // carry-in

    // carry[0] = cin, carry[i+1] = cout of bit i, carry[64] = final cout
    logic [64:0] carry;

    assign carry[0] = cin;  // feed cin

    genvar i;
    generate
        for(i=0; i<64; i=i+1) begin : ripple
            full_adder fa(sum[i], carry[i+1], a[i], b[i], carry[i]);
        end
    endgenerate

    assign cout = carry[64];

endmodule