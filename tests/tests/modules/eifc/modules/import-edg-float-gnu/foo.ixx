export module foo;

export __float128 quad(__float128 v) { return v * 4; }
export _Float32x f32x_half(_Float32x v) { return v / 2; }
export _Float64x f64x_half(_Float64x v) { return v / 2; }

export int kind(double) { return 1; }
export int kind(_Float32x) { return 2; }
export int kind(long double) { return 3; }
export int kind(_Float64x) { return 4; }
export int kind(__float128) { return 5; }
