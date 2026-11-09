//type:fn
//options:--c++20 --g++ --gnu_version=150200:--c++20 --clang --clang_version=220100
//remark:Reading an element of a destroyed GNU vector is not constant.

// Destroying a vector subobject used to mark only its first element as
// uninitialized, so reading a later element was accepted (d3).

typedef int vi4 __attribute__((vector_size(16)));
typedef long long vl8 __attribute__((vector_size(64)));

constexpr int d1() { vi4 a = {1, 2, 3, 4}; a.~vi4(); return a[3]; }
static_assert(d1() == 4, "");  // Error.

constexpr int d2() { vl8 a = vl8(); a.~vl8(); return (int)a[7]; }
static_assert(d2() == 0, "");  // Error.

struct S { vl8 v; };
constexpr int d3() { S s{}; s.v.~vl8(); return (int)s.v[7]; }
static_assert(d3() == 0, "");  // Error.
