//type:fn
//options:--c++11 --gnu_version 150000:--c++11 --clang_version 190000
//remark:[GH #356] C++11 constexpr template with a C++14-style body
//
// In GNU and Clang C++11 modes, a statement that C++14 allows in a constexpr
// function template only gets a warning.  The function can still be used in
// constant expressions.  A goto (not allowed in C++14 either) still makes
// the function unusable in constant expressions.

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
constexpr int f(T t) {
  if (t > 0) {}
  switch (sizeof(T)) { default: break; }
  return 1;
}
static_assert(f(1) == 1, "");

template <class T>
constexpr int g(T t) {
  goto done;
done:
  return 1;
}
static_assert(g(1) == 1, "");  // Error: g is not usable here.
