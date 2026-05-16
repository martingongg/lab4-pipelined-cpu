`timescale 1ps/1ps

// 64-bit-wide 3:1 mux.
// 2-bit selector encoding:
//   00 -> i00
//   01 -> i01
//   10 -> i10
//   11 -> i11  X
// Built from two layer of mux2_1 per bit:
//   first stage:  pick i00 vs i01 with sel[0]
//                 pick i10 vs i11 with sel[0]
//   second stage: pick low_pair vs high_pair with sel[1]

module mux3_64(out, i00, i01, i10, i11, sel);

    output [63:0] out;
    input  [63:0] i00, i01, i10, i11;
    input  [1:0]  sel;

    logic [63:0] low_pair, high_pair;

    genvar i;
    generate
        for (i = 0; i < 64; i = i + 1) begin : bits
            mux2_1 m_lo (.out(low_pair[i]),  .i0(i00[i]), .i1(i01[i]), .sel(sel[0]));
            mux2_1 m_hi (.out(high_pair[i]), .i0(i10[i]), .i1(i11[i]), .sel(sel[0]));
            mux2_1 m_top(.out(out[i]), .i0(low_pair[i]),  .i1(high_pair[i]), .sel(sel[1]));
        end
    endgenerate

endmodule
