`timescale 1ps/1ps

// multiply by 4) for branch offsets.
//drop top 2 bits, append 2 zeros 
// out = {in[61:0], 2'b00}

module shift2(in, out);
    input  [63:0] in;
    output [63:0] out;

    assign out = {in[61:0], 2'b00};

endmodule
