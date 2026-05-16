`timescale 1ps/1ps
// 2:4 Decoder: asserts exactly one of 4 outputs based on a 2-bit input
// Output is suppressed entirely when en=0
module decoder_2_4(out, en, in);
    output logic [3:0] out;
    input  logic en;
    input  logic [1:0] in;
    
    wire neg0, neg1;

    not #(50) not0 (neg0, in[0]);
    not #(50) not1 (neg1, in[1]);

    //                                       in[1] in[0]
    and #(50) g0 (out[0], en, neg1, neg0); // 0     0
    and #(50) g1 (out[1], en, neg1, in[0]); // 0     1
    and #(50) g2 (out[2], en, in[1], neg0); // 1     0
    and #(50) g3 (out[3], en, in[1], in[0]); // 1     1

endmodule