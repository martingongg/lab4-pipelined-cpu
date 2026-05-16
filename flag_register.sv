`timescale 1ps/1ps

// Flag register: 4 D_FFs with a shared write-enable.
// When en=1 (UpdateFlags), latches the live ALU flags on the next clock edge.
// When en=0, holds the previous stored value (mux feedback path).

module flag_register(
    clk, reset, en,
    alu_negative, alu_zero, alu_overflow, alu_carry_out,
    negative, zero, overflow, carry_out
);
    input  logic clk, reset, en;
    input  logic alu_negative, alu_zero, alu_overflow, alu_carry_out;  // live from ALU
    output logic negative, zero, overflow, carry_out; // stored

    // Each flag: i0 = feedback of stored value (hold), i1 = live ALU flag (write)
    // Negative
    logic d_neg;
    mux2_1 mux_n(.out(d_neg), .i0(negative), .i1(alu_negative), .sel(en));   // i0 feedback
    D_FF   ff_n (.q(negative), .d(d_neg), .reset(reset), .clk(clk));

    // Zero
    logic d_zero;
    mux2_1 mux_z(.out(d_zero), .i0(zero), .i1(alu_zero), .sel(en));// i0 feedback
    D_FF   ff_z (.q(zero), .d(d_zero), .reset(reset), .clk(clk));

    // Overflow
    logic d_ov;
    mux2_1 mux_v(.out(d_ov), .i0(overflow), .i1(alu_overflow), .sel(en));// i0 feedback
    D_FF   ff_v (.q(overflow), .d(d_ov), .reset(reset), .clk(clk));

    // Carry out
    logic d_co;
    mux2_1 mux_c(.out(d_co), .i0(carry_out), .i1(alu_carry_out), .sel(en));// i0 feedback
    D_FF   ff_c (.q(carry_out), .d(d_co), .reset(reset), .clk(clk));

endmodule
