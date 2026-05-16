`timescale 1ps/1ps

// N-bit pipeline register built from the same D_FF cells used in Lab 3.
module pipeline_register #(parameter N = 64) (out, in, reset, clk);

    output logic [N-1:0] out;
    input  logic [N-1:0] in;
    input  logic         reset, clk;

    genvar i;
    generate
        for (i = 0; i < N; i = i + 1) begin : bits
            D_FF dff(.q(out[i]), .d(in[i]), .reset(reset), .clk(clk));
        end
    endgenerate

endmodule
