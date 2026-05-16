`timescale 1ps/1ps

// 64-bit Program Counter register
// positive edge
module pc(in, out, reset, clk);
    input  [63:0] in;
    output [63:0] out;
    input reset, clk;

    genvar i;
    generate
        for (i = 0; i < 64; i = i + 1) begin : bits
            D_FF dff(.q(out[i]), .d(in[i]), .reset(reset), .clk(clk));
        end
    endgenerate

endmodule
