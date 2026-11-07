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
