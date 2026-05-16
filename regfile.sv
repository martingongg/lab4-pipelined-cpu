// EE469 Lab 1 - ARM Register File
// Andy Wu and Martin Gong
// April 10th 2026
`timescale 1ps/1ps

module regfile (ReadData1, ReadData2, WriteData,
                ReadRegister1, ReadRegister2, WriteRegister,
                RegWrite, clk);

    input  logic [4:0]  ReadRegister1, ReadRegister2, WriteRegister;
    input  logic [63:0] WriteData;
    input  logic        RegWrite, clk;
    output logic [63:0] ReadData1, ReadData2;

    logic [63:0][31:0] ffout;  // [bit][register] — already correct for mux
    logic [31:0]       fromDecoder;
    logic [63:0][30:0] d;      // mux outputs before DFF, registers 0-30 only

    // decoder
    decoder_5_32 dec (
        .en(RegWrite),
        .in(WriteRegister),
        .out(fromDecoder)
    );

    // Two read muxes, one per read port.
    // Each mux64_32 contains 64 copies of mux32_1 (one per bit position).
    // For each bit position, it selects the corresponding bit of the target register
    // from ffout and assembles the full 64-bit output on ReadData1/ReadData2.
    mux64_32 rd1 (.out(ReadData1), .in(ffout), .sel(ReadRegister1));
    mux64_32 rd2 (.out(ReadData2), .in(ffout), .sel(ReadRegister2));

    // registers 0-30: mux2_1 + D_FF per bit
    genvar i, j;
    generate
        for (i = 0; i < 31; i++) begin : gen_reg
            for (j = 0; j < 64; j++) begin : gen_dff
                mux2_1 m (
                    .out(d[j][i]),
                    .i0(ffout[j][i]),   // hold
                    .i1(WriteData[j]),  // write
                    .sel(fromDecoder[i])
                );
                D_FF dff (
                    .q(ffout[j][i]),
                    .d(d[j][i]),
                    .reset(1'b0),
                    .clk(clk)
                );
            end
        end
    endgenerate

    // register 31 hardwired to zero — assign is just wiring, no logic
    genvar k;
    generate
        for (k = 0; k < 64; k++) begin : gen_reg31
            assign ffout[k][31] = 1'b0;
        end
    endgenerate

endmodule