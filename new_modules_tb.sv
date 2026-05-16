`timescale 1ps/1ps

module pipeline_register_tb();
    logic clk, reset;
    logic [7:0] in, out;

    pipeline_register #(.N(8)) dut(.out(out), .in(in), .reset(reset), .clk(clk));

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        reset = 1'b1; in = 8'hA5; @(posedge clk); #1;
        assert(out == 8'h00) else $error("pipeline_register reset failed: out=%h", out);

        reset = 1'b0; in = 8'h3C; @(posedge clk); #1;
        assert(out == 8'h3C) else $error("pipeline_register latch failed: out=%h", out);

        $display("pipeline_register_tb passed");
        $finish;
    end
endmodule

module IF_ID_Reg_tb();
    logic clk, reset;
    logic [63:0] FetchPC, DecPC;
    logic [31:0] FetchInst, DecInst;

    IF_ID_Reg dut(.clk(clk), .reset(reset), .FetchPC(FetchPC),
                  .FetchInst(FetchInst), .DecPC(DecPC), .DecInst(DecInst));

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        reset = 1'b1; FetchPC = 64'h40; FetchInst = 32'hAA55_F00D; @(posedge clk); #1;
        assert(DecPC == 64'd0 && DecInst == 32'd0) else $error("IF_ID_Reg reset failed");

        reset = 1'b0; FetchPC = 64'h88; FetchInst = 32'h1234_5678; @(posedge clk); #1;
        assert(DecPC == 64'h88 && DecInst == 32'h1234_5678) else $error("IF_ID_Reg latch failed");

        $display("IF_ID_Reg_tb passed");
        $finish;
    end
endmodule

module ID_EX_Reg_tb();
    logic clk, reset;
    logic [63:0] DecIncrementedPC, DecDa, DecDb, DecImm12Ext, DecImm9Ext;
    logic [4:0]  DecAw;
    logic [2:0]  DecALUOp;
    logic [1:0]  DecALUSrc, DecMem2Reg;
    logic        DecRegWrite, DecMemWrite, DecMemRead, DecFlagWrite;
    logic [63:0] ExIncrementedPC, ExDa, ExDb, ExImm12Ext, ExImm9Ext;
    logic [4:0]  ExAw;
    logic [2:0]  ExALUOp;
    logic [1:0]  ExALUSrc, ExMem2Reg;
    logic        ExRegWrite, ExMemWrite, ExMemRead, ExFlagWrite;

    ID_EX_Reg dut(.*);

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        reset = 1'b1;
        {DecRegWrite, DecMemWrite, DecMemRead, DecFlagWrite} = 4'b1111;
        DecIncrementedPC = 64'h10; DecDa = 64'h11; DecDb = 64'h22;
        DecImm12Ext = 64'h123; DecImm9Ext = 64'h1FF; DecAw = 5'd9;
        DecALUOp = 3'b010; DecALUSrc = 2'b10; DecMem2Reg = 2'b01;
        @(posedge clk); #1;
        assert(ExIncrementedPC == 64'd0 && ExAw == 5'd0 && ExRegWrite == 1'b0) else $error("ID_EX_Reg reset failed");

        reset = 1'b0;
        @(posedge clk); #1;
        assert(ExIncrementedPC == 64'h10 && ExDa == 64'h11 && ExDb == 64'h22) else $error("ID_EX_Reg data latch failed");
        assert(ExImm12Ext == 64'h123 && ExImm9Ext == 64'h1FF && ExAw == 5'd9) else $error("ID_EX_Reg imm/addr latch failed");
        assert(ExALUOp == 3'b010 && ExALUSrc == 2'b10 && ExMem2Reg == 2'b01) else $error("ID_EX_Reg control latch failed");
        assert(ExRegWrite && ExMemWrite && ExMemRead && ExFlagWrite) else $error("ID_EX_Reg 1-bit control latch failed");

        $display("ID_EX_Reg_tb passed");
        $finish;
    end
endmodule

module EX_MEM_Reg_tb();
    logic clk, reset;
    logic [63:0] ExIncrementedPC, ExDb, ExALUOut;
    logic [4:0]  ExAw;
    logic [1:0]  ExMem2Reg;
    logic        ExRegWrite, ExMemWrite, ExMemRead;
    logic [63:0] MemIncrementedPC, MemDb, MemALUOut;
    logic [4:0]  MemAw;
    logic [1:0]  MemMem2Reg;
    logic        MemRegWrite, MemMemWrite, MemMemRead;

    EX_MEM_Reg dut(.*);

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        reset = 1'b1;
        ExIncrementedPC = 64'h20; ExDb = 64'hCAFE; ExALUOut = 64'hBEEF;
        ExAw = 5'd12; ExMem2Reg = 2'b10; {ExRegWrite, ExMemWrite, ExMemRead} = 3'b111;
        @(posedge clk); #1;
        assert(MemIncrementedPC == 64'd0 && MemAw == 5'd0 && MemRegWrite == 1'b0) else $error("EX_MEM_Reg reset failed");

        reset = 1'b0; @(posedge clk); #1;
        assert(MemIncrementedPC == 64'h20 && MemDb == 64'hCAFE && MemALUOut == 64'hBEEF) else $error("EX_MEM_Reg data latch failed");
        assert(MemAw == 5'd12 && MemMem2Reg == 2'b10) else $error("EX_MEM_Reg control latch failed");
        assert(MemRegWrite && MemMemWrite && MemMemRead) else $error("EX_MEM_Reg bit latch failed");

        $display("EX_MEM_Reg_tb passed");
        $finish;
    end
endmodule

module MEM_WB_Reg_tb();
    logic clk, reset;
    logic MemRegWrite, WbRegWrite;
    logic [1:0] MemMem2Reg;
    logic [4:0] MemAw, WbAw;
    logic [63:0] MemIncrementedPC, MemOut, MemALUOut, WbDataToReg, WbMuxOut;

    MEM_WB_Reg dut(.*);

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        reset = 1'b1; MemRegWrite = 1'b1; MemAw = 5'd30;
        MemALUOut = 64'hAA; MemOut = 64'hBB; MemIncrementedPC = 64'hCC;
        MemMem2Reg = 2'b00; @(posedge clk); #1;
        assert(WbDataToReg == 64'd0 && WbAw == 5'd0 && WbRegWrite == 1'b0) else $error("MEM_WB_Reg reset failed");

        reset = 1'b0; MemMem2Reg = 2'b00; @(posedge clk); #1;
        assert(WbDataToReg == 64'hAA && WbMuxOut == 64'hAA) else $error("MEM_WB_Reg ALU select failed");

        MemMem2Reg = 2'b01; @(posedge clk); #1;
        assert(WbDataToReg == 64'hBB && WbMuxOut == 64'hBB) else $error("MEM_WB_Reg memory select failed");

        MemMem2Reg = 2'b10; @(posedge clk); #1;
        assert(WbDataToReg == 64'hCC && WbAw == 5'd30 && WbRegWrite) else $error("MEM_WB_Reg PC select failed");

        $display("MEM_WB_Reg_tb passed");
        $finish;
    end
endmodule

module ForwardingUnit_tb();
    logic [4:0] DecAa, DecAb, ExAw, MemAw;
    logic ExRegWrite, MemRegWrite;
    logic [1:0] ForwardA, ForwardB;

    ForwardingUnit dut(.*);

    initial begin
        DecAa = 5'd1; DecAb = 5'd2; ExAw = 5'd1; MemAw = 5'd2;
        ExRegWrite = 1'b1; MemRegWrite = 1'b1; #1;
        assert(ForwardA == 2'b01 && ForwardB == 2'b10) else $error("ForwardingUnit EX/MEM forward failed");

        ExAw = 5'd31; MemAw = 5'd1; #1;
        assert(ForwardA == 2'b10) else $error("ForwardingUnit X31 exclusion failed");

        ExRegWrite = 1'b0; MemRegWrite = 1'b0; #1;
        assert(ForwardA == 2'b00 && ForwardB == 2'b00) else $error("ForwardingUnit no-forward failed");

        $display("ForwardingUnit_tb passed");
        $finish;
    end
endmodule

module ControlSignal_tb();
    logic [31:0] Instruction;
    logic NegativeFlag, OverflowFlag, ZeroFlag, BLTBrTaken;
    logic [2:0] ALUOp;
    logic [1:0] ALUSrc, Mem2Reg;
    logic BrTaken, BrSrc, Reg2Loc, Reg2Write, RegWrite, MemWrite, MemRead, UncondBr, FlagWrite;

    ControlSignal dut(.*);

    initial begin
        NegativeFlag = 1'b0; OverflowFlag = 1'b0; ZeroFlag = 1'b0; BLTBrTaken = 1'b0;

        Instruction = {11'b10010001000, 21'd0}; #1; // ADDI
        assert(RegWrite && ALUOp == 3'b010 && ALUSrc == 2'b10 && Mem2Reg == 2'b00) else $error("ControlSignal ADDI failed");

        Instruction = {11'b11111000010, 21'd0}; #1; // LDUR
        assert(RegWrite && MemRead && ALUSrc == 2'b01 && Mem2Reg == 2'b01) else $error("ControlSignal LDUR failed");

        Instruction = {11'b11111000000, 21'd0}; #1; // STUR
        assert(MemWrite && !RegWrite && ALUSrc == 2'b01 && Reg2Loc == 1'b0) else $error("ControlSignal STUR failed");

        Instruction = {11'b10101011000, 21'd0}; #1; // ADDS
        assert(RegWrite && FlagWrite && Reg2Loc && ALUSrc == 2'b00) else $error("ControlSignal ADDS failed");

        Instruction = {11'b10010100000, 21'd0}; #1; // BL
        assert(BrTaken && UncondBr && RegWrite && Reg2Write && Mem2Reg == 2'b10) else $error("ControlSignal BL failed");

        Instruction = {11'b10110100000, 21'd0}; ZeroFlag = 1'b1; #1; // CBZ
        assert(BrTaken && !UncondBr) else $error("ControlSignal CBZ failed");

        Instruction = {11'b01010100000, 21'd0}; BLTBrTaken = 1'b1; #1; // B.LT
        assert(BrTaken && !UncondBr) else $error("ControlSignal B.LT failed");

        $display("ControlSignal_tb passed");
        $finish;
    end
endmodule

module Execute_tb();
    logic clk, reset;
    logic [63:0] ExDa, ExDb, ExImm12Ext, ExImm9Ext, ExALUOut;
    logic [2:0] ExALUOp;
    logic [1:0] ExALUSrc;
    logic ExFlagWrite, ExOverflow, ExNegative, ExZero, ExCarryout, BLTBrTaken;

    Execute dut(.*);

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        reset = 1'b1; ExFlagWrite = 1'b0; ExDa = 0; ExDb = 0; ExImm9Ext = 0; ExImm12Ext = 0;
        ExALUOp = 3'b010; ExALUSrc = 2'b00; @(posedge clk); #1;

        reset = 1'b0; ExDa = 64'd5; ExDb = 64'd7; ExALUSrc = 2'b00; ExALUOp = 3'b010; #200;
        assert(ExALUOut == 64'd12) else $error("Execute register add failed: %h", ExALUOut);

        ExImm9Ext = 64'hFFFF_FFFF_FFFF_FFF8; ExALUSrc = 2'b01; #200;
        assert(ExALUOut == 64'hFFFF_FFFF_FFFF_FFFD) else $error("Execute imm9 select failed: %h", ExALUOut);

        ExImm12Ext = 64'd20; ExALUSrc = 2'b10; #200;
        assert(ExALUOut == 64'd25) else $error("Execute imm12 select failed: %h", ExALUOut);

        ExDa = 64'd3; ExDb = 64'd5; ExALUSrc = 2'b00; ExALUOp = 3'b011; ExFlagWrite = 1'b1; @(posedge clk); #200;
        assert(ExNegative && BLTBrTaken) else $error("Execute flag/BLT failed");

        $display("Execute_tb passed");
        $finish;
    end
endmodule

module Memory_tb();
    logic clk, reset, MemWrite, MemRead;
    logic [63:0] address, MemWriteData, MemOut;

    Memory dut(.*);

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        reset = 1'b0; address = 64'd16; MemWriteData = 64'h0123_4567_89AB_CDEF;
        MemRead = 1'b0; MemWrite = 1'b1; @(posedge clk); #1;

        MemWrite = 1'b0; MemRead = 1'b1; #1;
        assert(MemOut == 64'h0123_4567_89AB_CDEF) else $error("Memory read/write failed: %h", MemOut);

        $display("Memory_tb passed");
        $finish;
    end
endmodule

module InstructionFetch_tb();
    logic clk, reset, brTaken;
    logic [63:0] branchAddress, currentPC;
    logic [31:0] Instruction;

    InstructionFetch dut(.*);

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        reset = 1'b1; brTaken = 1'b0; branchAddress = 64'd64; @(posedge clk); #1;
        assert(currentPC == 64'd0) else $error("InstructionFetch reset failed");

        reset = 1'b0; @(posedge clk); #1;
        assert(currentPC == 64'd4) else $error("InstructionFetch PC+4 failed: %h", currentPC);

        brTaken = 1'b1; branchAddress = 64'd80; @(posedge clk); #1;
        assert(currentPC == 64'd80) else $error("InstructionFetch branch failed: %h", currentPC);

        $display("InstructionFetch_tb passed");
        $finish;
    end
endmodule

module InstructionDecode_tb();
    logic clk, reset;
    logic DecReg2Loc, DecReg2Write, DecUncondBr, DecBrSrc, WbRegWrite, DecZero;
    logic [63:0] DecPC, WbDataToReg, ExALUOut, WbMuxOut;
    logic [31:0] DecInst;
    logic [4:0] WbAw, DecAa, DecAb, DecAw;
    logic [1:0] ForwardA, ForwardB;
    logic [63:0] DecDa, DecDb, DecImm12Ext, DecImm9Ext, DecBranchPC;

    InstructionDecode dut(.*);

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        reset = 1'b0; WbRegWrite = 1'b0; WbAw = 5'd0; WbDataToReg = 64'd0;
        DecPC = 64'd100; DecReg2Loc = 1'b1; DecReg2Write = 1'b1;
        DecUncondBr = 1'b0; DecBrSrc = 1'b0;
        ForwardA = 2'b01; ForwardB = 2'b10; ExALUOut = 64'h1111; WbMuxOut = 64'h2222;
        DecInst = 32'd0;
        DecInst[9:5] = 5'd3;
        DecInst[20:16] = 5'd4;
        DecInst[4:0] = 5'd5;
        #10;

        assert(DecAa == 5'd3 && DecAb == 5'd4 && DecAw == 5'd30) else $error("InstructionDecode register address mux failed");
        assert(DecDa == 64'h1111 && DecDb == 64'h2222) else $error("InstructionDecode forwarding failed");

        DecInst = 32'd0;
        DecInst[21:10] = 12'hABC;
        DecInst[20:12] = 9'h1F0;
        #10;
        assert(DecImm12Ext == 64'hABC && DecImm9Ext == 64'hFFFF_FFFF_FFFF_FFF0) else $error("InstructionDecode immediates failed");

        DecInst = 32'd0;
        DecInst[23:5] = 19'd3;
        #10;
        assert(DecBranchPC == 64'd112) else $error("InstructionDecode branch target failed: %h", DecBranchPC);

        DecBrSrc = 1'b1; #10;
        assert(DecBranchPC == 64'h2222) else $error("InstructionDecode BR source failed");

        ForwardB = 2'b10; WbMuxOut = 64'd0; #10;
        assert(DecZero == 1'b1) else $error("InstructionDecode zero detect failed");

        WbMuxOut = 64'd7; #10;
        assert(DecZero == 1'b0) else $error("InstructionDecode nonzero detect failed");

        $display("InstructionDecode_tb passed");
        $finish;
    end
endmodule
