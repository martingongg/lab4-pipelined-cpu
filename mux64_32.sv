`timescale 1ps/1ps

// 64-bit wide 32:1 multiplexor.
// Selects one 64-bit register out of 32, one bit at a time.
// "in" holds all 32 register values across all 64 bit positions: in[bit][reg].
// "sel" is a 5-bit register index (0-31).
// "out" is the full 64-bit value of the selected register.
// Internally instantiates 64 copies of mux32_1 — one per bit position.
module mux64_32(out, in, sel);
    output logic [63:0] out;
    input logic [63:0][31:0] in;  // 64 bit positions, each has 32 register values
    input logic [4:0] sel;

    // Generate one mux32_1 per bit position:
    // each instance selects the correct register's bit using the shared sel signal
    genvar i; //Declaring i as a generate variable
    generate
        for(i=0; i<64; i=i+1) begin : eachBit // Loop runs 64 times, creates 64 separate instances
            mux32_1 m(.out(out[i]), .in(in[i][31:0]), .sel(sel));
        end
    endgenerate

endmodule