`timescale 1ps/1ps
// 2:1 Multiplexer: selects one of two inputs based on select signal
// Boolean equation: out = (~sel)·i0 + sel·i1
//   - When sel = 0  output = i0
//   - When sel = 1  output = i1
module mux2_1(out, i0, i1, sel);
    output logic out;
    input logic i0, i1, sel;
    wire notSel, t0, t1;
    
    not #(50) g1(notSel, sel);
    and #(50) g2(t0, i0, notSel);
    and #(50) g3(t1, i1, sel);
    or  #(50) g4(out, t0, t1);
endmodule