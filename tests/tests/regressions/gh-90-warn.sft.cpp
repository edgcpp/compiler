//type:fp
//options_all:--c++20
//options::--g++ --gnu_version=150000:--g++ --gnu_version=80000:--clang --clang_version=190100

// Returning a reference or pointer into a member of a class prvalue returns
// a reference into a local temporary.  When "A().b" is an xvalue (Core issue
// 616), that is still diagnosed, as it is when "A().b" is a prvalue.

struct A { int b; ~A(); };
struct R { int &r; int *p; };
int g;

const int &f1() { return A().b; }                            // Warning.
const int *f2() { return &static_cast<const int &>(A().b); } // Warning.
int &&f3() { return static_cast<A &&>(A()).b; }              // Warning.

// No warnings for these.
const int &n1() { return R{g, &g}.r; }
const int &n2() { return *R{g, &g}.p; }
const int &n3(A &&a) { return static_cast<A &&>(a).b; }
int n4() { return A().b; }
