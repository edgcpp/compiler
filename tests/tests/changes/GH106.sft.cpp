//type:fn
//options:--c++26:--c++26 --gnu_version 140000:--c++26 --clang_version 190000
//remark:[GH #106] C++26: returning a reference bound to a temporary
//
// P2748 makes a return statement that binds the returned reference to a
// temporary ill-formed in C++26.  Returning a reference to a local variable
// is still only a warning.

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
using A = int[2];
A&& f9() { return A{1, 2}; }              // Array temporary.
const A& f10() { return A{1, 2}; }        // Array temporary.
const int& f11() { return A{1, 2}[0]; }   // Element of an array temporary.
const int& f12(int n) { return A{n, 2}[1]; }
const int& f13() { return 0[A{1, 2}]; }

// Core issue 2352: the reference binds directly to pp, so this is not
// diagnosed as an error.
int **pp;
const int *const *const &f6() { return pp; }

// P2748 does not apply to the invented function used for is_convertible.
static_assert(__is_convertible_to(int, const int&), "");
static_assert(__is_convertible_to(long, const int&), "");
static_assert(__is_convertible_to(S, const S&), "");
