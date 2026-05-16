`timescale 1ps/1ps

// Sign-extend a 26-bit value to 64 bits.
// Used for B and BL (Imm26).

module signExtend26 (in, out);
    input  logic [25:0] in;
    output logic [63:0] out;

    assign out[25:0] = in;

    genvar i;
    generate
        for (i = 26; i < 64; i++) begin : bits
            assign out[i] = in[25];
        end
    endgenerate
endmodule
