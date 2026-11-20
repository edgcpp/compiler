export module foo;

// In Clang 14 mode, _Float16 is the extension type, not the standard one.
export _Float16 half(_Float16 v) { return v / 2; }
// constexpr, so no code for __fp16 (which GCC rejects on x86) is generated.
export constexpr __fp16 fp16_id(__fp16 v) { return v; }
export __float128 quad(__float128 v) { return v * 4; }
export __bf16 bf16_id(__bf16 v) { return v; }

export constexpr int kind(_Float16) { return 1; }
export constexpr int kind(__fp16) { return 2; }
export constexpr int kind(__float128) { return 3; }
export constexpr int kind(long double) { return 4; }

export struct holder {
  _Float16 a;
  __fp16 b;
  __float128 c;
  __bf16 d;
};
