//type:fn
//options:--c++26:--c++26 --gnu_version 150000:--c++26 --clang_version 200000
//remark:[GH #370] C++26: comparison between two arrays is not allowed
//
// P2865 makes a comparison between two arrays ill-formed in C++26.

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

// A conversion function result compared with a const array: the operand
// also goes through a qualification conversion.
const int ca[2] = {};
bool h1(C c) { return c == ca; }              // Two arrays: diagnosed.
bool h2() { return (const int *)a == ca; }    // Explicit cast: not diagnosed.
