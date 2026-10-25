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

// Core issue 2352: the reference binds directly to pp, so this is not
// diagnosed as an error.
int **pp;
const int *const *const &f6() { return pp; }
