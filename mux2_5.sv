`timescale 1ps/1ps

// 5-bit-wide 2:1 mux. Used for Reg2Loc (Rd vs Rm) and BLwrite (Rd vs 5'd30).
// out = sel ? i1 : i0

module mux2_5(out, i0, i1, sel);

    output [4:0] out;
    input  [4:0] i0, i1;
    input        sel;

    genvar i;
    generate
        for (i = 0; i < 5; i = i + 1) begin : bits
            mux2_1 m(.out(out[i]), .i0(i0[i]), .i1(i1[i]), .sel(sel));
        end
    endgenerate

endmodule
