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
