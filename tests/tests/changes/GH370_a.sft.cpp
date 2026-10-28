//type:fp
//options:--c++20:--c++26 --gnu_version 140000:--c++26 --clang_version 190000:--c++17
//remark:[GH #370] Comparison between two arrays is deprecated in C++20
//
// A comparison between two arrays is deprecated in C++20 and C++23.  When
// emulating GCC before 15 or Clang before 20, it is also only deprecated in
// C++26.  Before C++20 it is not diagnosed.

int a[2], b[2];
int *p = b;

bool x1 = a == b;  // Two arrays: diagnosed.
bool x2 = a != b;
bool x3 = a < b;
bool x4 = a >= b;

bool y1 = a == p;  // Array and pointer: not diagnosed.
bool y2 = p < a;

// Class operands converted to arrays by conversion functions.
struct C { using arr_t = int[2]; operator arr_t &() const; };
struct P { operator int *() const; };
void g(C c, P q) {
  bool z1 = c == c;    // Two arrays: diagnosed.
  bool z2 = c < c;
  bool z3 = c == a;
  bool z4 = c == p;    // Array and pointer: not diagnosed.
  bool z5 = q == q;    // Conversions to pointers: not diagnosed.
  bool z6 = +c == +c;  // Explicit decay: not diagnosed.
}
