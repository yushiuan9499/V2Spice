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
