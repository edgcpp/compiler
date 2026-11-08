//type:fn
//options:--c++20 --g++ --gnu_version=150200:--c++20 --clang --clang_version=190100
//remark:Reading an uninitialized element of a GNU vector is not constant.

// Storing or copying a whole vector marks each of its elements initialized,
// so reading a whole vector requires each element to be initialized.

typedef int v4 __attribute__((vector_size(16)));
struct X { v4 v; };
struct Z { v4 v; constexpr Z(const v4 &a) : v(a) {} };

constexpr int e1() { X x; x.v[0] = 1; return x.v[2]; }
static_assert(e1() == 0);  // Error.

constexpr int e2() { X x; x.v[0] = 1; Z z(x.v); return z.v[3]; }
static_assert(e2() == 0);  // Error.

constexpr int e3() { X x; x.v[0] = 1; X y; y.v = x.v; return y.v[3]; }
static_assert(e3() == 0);  // Error.

constexpr int e4() { X x; x.v[1] = 1; X y = x; return y.v[1]; }
static_assert(e4() == 1);  // Error.

constexpr int e5() { X x; x.v[0] = 1; X y = x; return y.v[0]; }
static_assert(e5() == 1);  // Error.
