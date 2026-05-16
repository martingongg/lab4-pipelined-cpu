`timescale 1ps/1ps

// Sign-extend a 19-bit value to 64 bits.
// Used for B.LT and CBZ (Imm19).

module signExtend19 (in, out);
    input  logic [18:0] in;
    output logic [63:0] out;

    assign out[18:0] = in;

    genvar i;
    generate
        for (i = 19; i < 64; i++) begin : bits
            assign out[i] = in[18];
        end
    endgenerate
endmodule
