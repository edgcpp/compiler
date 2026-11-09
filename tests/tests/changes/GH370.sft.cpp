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

// String literals are arrays too.
bool s1 = "abc" == "abc";

// Templates: the operands are arrays in the definition, or only after
// substitution.  Each comparison is diagnosed once.
template<class T> bool t1(T) { return a == b; }
template bool t1(int);
template bool t1(double);
template<class T> bool t2(T &x, T &y) { return x == y; }
template bool t2(int (&)[2], int (&)[2]);
template bool t2(int *&, int *&);             // Pointers: not diagnosed.
template<int N> bool t3(int (&x)[N], int (&y)[N]) { return x < y; }
template bool t3(int (&)[2], int (&)[2]);

// The comparison is ill-formed, so it is a substitution failure.
template<class T, class U>
concept EqComparable = requires(T &t, U &u) { t == u; };
static_assert(!EqComparable<int[2], int[2]>);
static_assert(EqComparable<int *, int[2]>);

template<class T>
constexpr auto eq(T &x, T &y) -> decltype(x == y, 1) { return 1; }
constexpr int eq(...) { return 2; }
static_assert(eq(a, b) == 2);
static_assert(eq(p, p) == 1);
