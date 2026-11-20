export module foo;

// In GNU 12 mode, _Float16 is the extension type, not the standard one.
// __fp16 is covered by the GNU 15 and Clang tests: GCC rejects it on x86.
export _Float16 half(_Float16 v) { return v / 2; }
export __float80 f80_id(__float80 v) { return v; }
export __float128 quad(__float128 v) { return v * 4; }
export __bf16 bf16_id(__bf16 v) { return v; }

export constexpr int kind(_Float16) { return 1; }
export constexpr int kind(float) { return 2; }
export constexpr int kind(double) { return 3; }
export constexpr int kind(__float128) { return 4; }
export constexpr int kind(__bf16) { return 5; }

export __float128 *quad_ptr(__float128 *p) { return p; }
export const _Float16 *f16_cptr(const _Float16 *p) { return p; }
export _Float16 (*half_fn())(_Float16) { return half; }

export struct holder {
  _Float16 a;
  __float128 c;
  __bf16 d;
  __float80 e;
  __float128 get_c() const { return c; }
};

export using quad_t = __float128;
export typedef __bf16 bf16_t;
