`timescale 1ps/1ps
// 3:8 Decoder: asserts exactly one of 8 outputs based on a 3-bit input
// Output is suppressed entirely when en=0
module decoder_3_8(out, en, in);
    output logic [7:0] out;
    input  logic       en;
    input  logic [2:0] in;

    wire neg0, neg1, neg2;

    not #(50) not0 (neg0, in[0]);
    not #(50) not1 (neg1, in[1]);
    not #(50) not2 (neg2, in[2]);

    //                                              in[2] in[1] in[0]
    and #(50) g0 (out[0], en, neg2, neg1, neg0); //  0     0     0
    and #(50) g1 (out[1], en, neg2, neg1, in[0]); //  0     0     1
    and #(50) g2 (out[2], en, neg2, in[1], neg0); //  0     1     0
    and #(50) g3 (out[3], en, neg2, in[1], in[0]); //  0     1     1
    and #(50) g4 (out[4], en, in[2], neg1, neg0); //  1     0     0
    and #(50) g5 (out[5], en, in[2], neg1, in[0]); //  1     0     1
    and #(50) g6 (out[6], en, in[2], in[1], neg0); //  1     1     0
    and #(50) g7 (out[7], en, in[2], in[1], in[0]); //  1     1     1

endmodule