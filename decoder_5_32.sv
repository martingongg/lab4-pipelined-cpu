`timescale 1ps/1ps
// 5:32 Decoder: asserts exactly one of 32 outputs based on a 5-bit input
// Built from two decoder_4_16 instances and gate logic to split on in[4]
module decoder_5_32(out, en, in);
    output logic [31:0] out;
    input  logic        en;
    input  logic [4:0]  in;

    wire en_lo, en_hi, neg4;

    // use in[4] to select which half is active
    not #(50) not4  (neg4,  in[4]);
    and #(50) g_lo  (en_lo, en, neg4);  // en_lo = en & ~in[4]
    and #(50) g_hi  (en_hi, en, in[4]); // en_hi = en &  in[4]

    // lower half: in[4]=0, decode in[3:0] → out[0..15]
    decoder_4_16 lo (.out(out[15:0]),  .en(en_lo), .in(in[3:0]));

    // upper half: in[4]=1, decode in[3:0] → out[16..31]
    decoder_4_16 hi (.out(out[31:16]), .en(en_hi), .in(in[3:0]));

endmodule