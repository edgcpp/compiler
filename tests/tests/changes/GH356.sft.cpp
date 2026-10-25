//type:fn
//options:--c++11 --gnu_version 150000:--c++11 --clang_version 190000
//remark:[GH #356] C++11 constexpr template with a C++14-style body
//
// In GNU and Clang C++11 modes, a statement that C++14 allows in a constexpr
// constructor of a template only gets a warning, and the constructor can
// still be used in constant expressions.  For a constexpr function template,
// that is only the case in Clang mode (GCC rejects the call).  A goto, which
// C++14 does not allow either, makes the function unusable in both modes.

template <class T>
struct A {
  T x;
  constexpr A(T i) : x(i) {
    if (sizeof(T) == 0) {}
  }
};
constexpr A<int> a(5);
static_assert(a.x == 5, "");

template <class T>
struct B {
  T x;
  constexpr B(T i) : x(i) { return; }
};
constexpr B<int> b(6);
static_assert(b.x == 6, "");

template <class T>
constexpr int f(T t) {
  if (t > 0) {}
  switch (sizeof(T)) { default: break; }
  return 1;
}
static_assert(f(1) == 1, "");  // Error in GNU mode only.

template <class T>
constexpr int g(T t) {
  goto done;
done:
  return 1;
}
static_assert(g(1) == 1, "");  // Error: g is not usable here.
