`timescale 1ps/1ps

// 64-bit wide 2:1 mux. 64 copies of mux2_1, one per bit.

module mux2_64(out, i0, i1, sel);

    output [63:0] out;
    input  [63:0] i0, i1;
    input         sel;

    genvar i;
    generate
        for (i = 0; i < 64; i = i + 1) begin : bits
            mux2_1 m(.out(out[i]), .i0(i0[i]), .i1(i1[i]), .sel(sel));
        end
    endgenerate

endmodule
