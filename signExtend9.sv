`timescale 1ps/1ps

// Sign-extend a 9-bit value to 64 bits.
// Used for LDUR/STUR (Imm9 address offset).

module signExtend9 (in, out);
    input  logic [8:0]  in;
    output logic [63:0] out;

    assign out[8:0] = in;

    genvar i;
    generate
        for (i = 9; i < 64; i++) begin : bits
            assign out[i] = in[8];
        end
    endgenerate
endmodule
