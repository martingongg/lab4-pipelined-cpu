`timescale 1ps/1ps

// 4:16 Decoder: asserts exactly one of 16 outputs based on a 4-bit input
// Built from two decoder_3_8 instances and gate logic to split on in[3]

module decoder_4_16(out, en, in);
    output logic [15:0] out;
    input  logic        en;
    input  logic [3:0]  in;

    wire en_lo, en_hi, neg3;

    // Use in[3] to determine which half is active
    not #(50) not3  (neg3,  in[3]);
    and #(50) g_lo  (en_lo, en, neg3);  // en_lo = en & ~in[3]
    and #(50) g_hi  (en_hi, en, in[3]); // en_hi = en &  in[3]

     // Lower half: in[3]=0, decode in[2:0] → out[7:0]
    decoder_3_8 lo (.out(out[7:0]),  .en(en_lo), .in(in[2:0]));

    // Upper half: in[3]=1, decode in[2:0] → out[15:8]
    decoder_3_8 hi (.out(out[15:8]), .en(en_hi), .in(in[2:0]));

endmodule