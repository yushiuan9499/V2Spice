module n_18 #(W=1u,L=18u)(
    D,
    G,
    S,
    B
);
endmodule

module p_18 #(W=1u,L=18u)(
    D,
    G,
    S,
    B
);
endmodule
module inv (
    in,
    out_1,
    GND,
    VDD
);
p_18 M1 #(W=1u,L=18u)(
    .D(out_1),
    .G(in),
    .B(GND),
    .S(VDD)
);
n_18 M2 #(W=0.5u,L=18u)(
    .G(in),
    .S(GND),
    .B(VDD),
    .D(out_1)
);
endmodule

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
