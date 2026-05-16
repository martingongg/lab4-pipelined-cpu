`timescale 1ps/1ps

module full_adder(sum, cout, a, b, cin);

    output sum, cout;
    input  a, b, cin;

    // CarryOut = (a·cin) + (b·cin) + (a·b)
    logic ac, bc, ab;
    and #(50) g1(ac, a,  cin);
    and #(50) g2(bc, b,  cin);
    and #(50) g3(ab, a,  b);
    or  #(50) g4(cout, ac, bc, ab);

    // Sum = a XOR b XOR cin
    logic axb;
    xor #(50) g5(axb, a, b);
    xor #(50) g6(sum, axb, cin);
endmodule