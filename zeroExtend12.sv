`timescale 1ps/1ps

// Zero-extend a 12-bit value to 64 bits.
// Used for ADDI (Imm12 is unsigned).

module zeroExtend12 (in, out);
    input  logic [11:0] in;
    output logic [63:0] out;

    assign out[11:0] = in;

    genvar i;
    generate
        for (i = 12; i < 64; i++) begin : bits
            assign out[i] = 1'b0;
        end
    endgenerate
endmodule
