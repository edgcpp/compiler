//type:fp
//options:--c++23:--c++26 --gnu_version 130000:--c++26 --clang_version 180000
//remark:[GH #106] Returning a reference bound to a temporary before C++26
//
// Before C++26, and in C++26 when emulating GCC before 14 or Clang before
// 19, returning a reference bound to a temporary is only a warning.

struct S { int x; };

const int& f1() { return 1; }             // Temporary: diagnosed.
const int& f2(long l) { return l; }       // Temporary from a conversion.
const int& f3() { return S().x; }         // Member of a temporary.
int&& f4() { return 1; }                  // Rvalue reference to a temporary.
const int& f5() { int i = 0; return i; }  // Local variable: only a warning.
S make();
const int& f7() { return make().x; }      // Member of a call's result.
int&& f8(bool b, int i) {                 // A "?" operand is a temporary.
  return b ? static_cast<int&&>(i) : S().x;
}

// Core issue 2352: the reference binds directly to pp, so this is not
// diagnosed as an error.
int **pp;
const int *const *const &f6() { return pp; }

// P2748 does not apply to the invented function used for is_convertible.
static_assert(__is_convertible_to(int, const int&), "");
static_assert(__is_convertible_to(long, const int&), "");
static_assert(__is_convertible_to(S, const S&), "");
