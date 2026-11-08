//type:fp
//options_all:--c++20
//options:--g++ --gnu_version=150200:--clang --clang_version=190100
// With Core issue 616, a GNU vector member of a class prvalue is an xvalue.
// Subscripting it in a constant expression must read the element.
typedef int v4 __attribute__((vector_size(16)));
struct X { v4 v; };
constexpr int f() { return X{{10, 20, 30, 40}}.v[3]; }
static_assert(f() == 40);
constexpr X mk() { return X{{1, 2, 3, 4}}; }
static_assert(mk().v[1] == 2);
constexpr int g() { v4 a = {1, 2, 3, 4}; return X{a * a}.v[3]; }
static_assert(g() == 16);

// A vector member initialized by a mem-initializer, and copies of objects
// with vector members.
struct Y { v4 v; constexpr Y(v4 a) : v(a) {} };
constexpr int h1() { v4 a = {1, 2, 3, 4}; return Y(a).v[3]; }
static_assert(h1() == 4);
constexpr int h2() { v4 a = {1, 2, 3, 4}; Y y(a); Y z = y; return z.v[2]; }
static_assert(h2() == 3);
union U { v4 v; int i; };
constexpr int h3() { U u{{1, 2, 3, 4}}; U w = u; return w.v[3]; }
static_assert(h3() == 4);
constexpr int h4() { v4 a = {1, 2, 3, 4}; return [a] { return a[3]; }(); }
static_assert(h4() == 4);
// An array of vectors.
constexpr int h5() { v4 a[2] = {{1, 2, 3, 4}, {5, 6, 7, 8}}; return a[1][3]; }
static_assert(h5() == 8);
// A vector member of a local object, initialized by a mem-initializer.
constexpr int h6() { v4 a = {1, 2, 3, 4}; Y y(a); return y.v[3]; }
static_assert(h6() == 4);
