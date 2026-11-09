//type:fp
//options:--c++11 --g++ --gnu_version=150000:--c++11 --clang --clang_version=190000
//remark:[GH #356] C++11 constexpr template accepted with a warning remains usable in constant expressions.

template <class Tp>
struct A {
  Tp x;
  constexpr A(Tp i) : x(i) {
    if (sizeof(Tp) == 0) {}
  }
};

template <int X>
struct V {
  static const int x = X;
};

template <class T, class U>
constexpr int test_it(U u) {
  return ((void)T(u), 1);
}

constexpr A<int> a(5);
static_assert(a.x == 5, "");
static_assert(V<test_it<A<int> >(5)>::x == 1, "");

template <class T>
constexpr int f(T x) {
  if (sizeof(T) == 0) {}
  return x + 1;
}

static_assert(f(41) == 42, "");
