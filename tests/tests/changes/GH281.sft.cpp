//type:fp
//options:--c++11:--c++11 --gnu_version 160200:--c++11 --clang_version 230100:--c++20:--c++20 --gnu_version 160200:--c++20 --clang_version 230100:--ms_c++20 --microsoft_version 1950
//options_all:-tused
//remark:Explicit conversion functions before aggregate fallback [GH #281]
// Core issue 899 permits these conversions in C++11 and later.  Previously,
// C++20 parenthesized aggregate initialization caused spurious errors.

struct D { float x, y; };
struct S {
  float x, y;
  explicit constexpr operator D() const { return D{x, y}; }
};

D f(S s) {
  D d(s);       // Previously an error in C++20 mode.  Now okay.
  return D(s);  // Likewise.
}

constexpr S s = {1.0f, 2.0f};
static_assert(D(s).x == 1.0f && D(s).y == 2.0f, "functional cast");
