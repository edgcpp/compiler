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
