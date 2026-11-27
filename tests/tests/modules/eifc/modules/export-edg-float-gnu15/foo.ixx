export module foo;

// In GNU 15 mode, _Float16/32/64/128 are standard types; _Float32x,
// _Float64x, __fp16, __float128, and __bf16 are extension types.
export _Float32x f32x_half(_Float32x v) { return v / 2; }
export _Float64x f64x_half(_Float64x v) { return v / 2; }
// constexpr, so no code for __fp16 (which GCC rejects on x86) is generated.
export constexpr __fp16 fp16_id(__fp16 v) { return v; }
export __float128 quad(__float128 v) { return v * 4; }
export __bf16 bf16_id(__bf16 v) { return v; }

// Each extension type stays distinct from the standard type of the same size.
export constexpr int kind(_Float16) { return 1; }
export constexpr int kind(__fp16) { return 2; }
export constexpr int kind(_Float32) { return 3; }
export constexpr int kind(_Float32x) { return 4; }
export constexpr int kind(_Float64) { return 5; }
export constexpr int kind(_Float64x) { return 6; }
export constexpr int kind(_Float128) { return 7; }
export constexpr int kind(__float128) { return 8; }
export constexpr int kind(__bf16) { return 9; }
export constexpr int kind(double) { return 10; }
export constexpr int kind(long double) { return 11; }

export struct holder {
  _Float32x a;
  _Float64x b;
  __fp16 c;
  __float128 d;
  __bf16 e;
};

export template <class T> constexpr int size_of() { return sizeof(T); }
export template <class T> struct box {
  T value;
};
export box<__float128> make_box() { return box<__float128>{2}; }
export box<_Float32x> make_box32x() { return box<_Float32x>{3}; }
