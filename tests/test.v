
`include "inv.v"
module no_port_and_use_array();
n_18 M1 #(W=1u*2,L=18u/2u*3u+1u)(
    .D(a[0]),
    .G(a[1]),
    .S(a[2]),
    .B(a[3])
);
endmodule

module lots_of_binary_op();
n_18 M1 #(W=1u*2,L=18u/2u*3u+1u)(
    .D(a),
    .G(b),
    .S(c),
    .B(d)
);
endmodule

$spice("V1 VDD 0 DC 1.8V");
