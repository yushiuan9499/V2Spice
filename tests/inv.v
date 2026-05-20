
module inv (
    in,
    out_1,
    GND,
    VDD
);
p_18 M1 #(W=1u,L=0.18u)(
    .D(out_1),
    .G(in),
    .B(VDD),
    .S(VDD)
);
n_18 M2 #(W=0.5u,L=0.18u)(
    .G(in),
    .S(GND),
    .B(GND),
    .D(out_1)
);
endmodule

