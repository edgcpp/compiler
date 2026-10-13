//type:fp
//options_all:--gnu_version=150000 --diag_error=inline_gnu_noinline_conflict
//options:--c++17
// GCC does not diagnose "inline" combined with "noinline" on a C++ function
// definition (unless an earlier declaration already made the function
// inline).  Boost.System and others use the idiom "BOOST_NOINLINE inline".
// (See gh-183-redecl.sft.cpp for definitions that follow a declaration
// without "inline".)
#define NOINLINE __attribute__((__noinline__))

inline __attribute__((noinline)) int f(int x) { return x; }
int g() { return f(1); }

NOINLINE inline int f2(int x) { return x; }
static inline NOINLINE int f4(int x) { return x; }
[[gnu::noinline]] inline int f5(int x) { return x; }
constexpr NOINLINE int f6(int x) { return x; }
namespace N { inline NOINLINE int f7(int x) { return x; } }

struct A {
  NOINLINE int m2(int x) { return x; }  // Implicitly inline.
  template <class T> T m3(T x);
  friend inline NOINLINE int fr(A) { return 0; }
};
template <class T> inline NOINLINE T A::m3(T x) { return x; }

template <class T> inline NOINLINE T t1(T x) { return x; }

template <class T> struct B {
  T m(T x);
};
template <class T> inline NOINLINE T B<T>::m(T x) { return x; }

auto lam = [](int x) NOINLINE { return x; };

int h() {
  A a;
  B<int> b;
  return f2(1) + f4(1) + f5(1) + f6(1) + N::f7(1) + a.m2(1) + a.m3(1) +
         fr(a) + t1(1) + t1(1L) + b.m(1) + lam(1);
}
