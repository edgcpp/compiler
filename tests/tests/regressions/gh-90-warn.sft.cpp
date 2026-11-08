//type:fp
//options:--c++20:--c++20 --g++ --gnu_version=150000:--c++20 --g++ --gnu_version=80000:--c++20 --clang --clang_version=190100:--c++11 --g++ --gnu_version=150000:--c++11 --clang --clang_version=190100

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

// A trivially destructible class prvalue is not represented by an explicit
// temporary, but a pointer into it still points into a temporary.
struct T { int b; int arr[2]; int *p; };
T make();

const int *f4() { return &static_cast<const int &>(make().b); } // Warning.
const int *f5() { return make().arr; }         // Warning (not with GCC 8).
const int *f6() { return &make().arr[1]; }     // Warning (not with GCC 8).

// No warnings for these.
int *n5() { return make().p; }
int *n6() { return &*make().p; }
