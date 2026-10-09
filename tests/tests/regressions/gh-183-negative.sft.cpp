//type:fn
//options_all:--gnu_version=150000 --diag_error=inline_gnu_noinline_conflict
//options:--c++17
// Cases in which GCC still diagnoses "inline" combined with "noinline" in C++
// (see gh-183.sft.cpp for the cases in which it does not): declarations that
// are not definitions, and redeclarations that add one to the other.
#define NOINLINE __attribute__((__noinline__))

inline NOINLINE int f1(int x);   // Error: not a definition.
inline int f1(int x) { return x; }   // Error: inline after noinline.

inline int f2(int x) NOINLINE;   // Error: not a definition.

NOINLINE int f3(int x);
inline int f3(int x) { return x; }   // Error: inline after noinline.

NOINLINE int f4(int x);
inline NOINLINE int f4(int x) { return x; }   // Error: inline after
                                              // noinline (only once).

inline int f5(int x);
NOINLINE int f5(int x) { return x; }   // Error: noinline after inline.

inline int f6(int x);
inline NOINLINE int f6(int x) { return x; }   // Error: noinline after
                                              // inline.

constexpr int f7(int x);
constexpr NOINLINE int f7(int x) { return x; }   // Error: noinline after
                                                 // inline.

struct A {
  inline NOINLINE int m1(int x);   // No error in the class.
  NOINLINE int m2(int x);
  inline int m3(int x);
};
inline NOINLINE int A::m1(int x) { return x; }   // Error.
inline NOINLINE int A::m2(int x) { return x; }   // Error (only once).
inline NOINLINE int A::m3(int x) { return x; }   // Error.

template <class T> struct B {
  NOINLINE T m1(T x);
  inline T m2(T x);
};
template <class T> inline T B<T>::m1(T x) { return x; }   // Error.
template <class T> NOINLINE T B<T>::m2(T x) { return x; }   // Error.

int g() {
  A a;
  B<int> b;   // Instantiating B<int>::m1 and m2 adds no further errors.
  return f1(1) + f3(1) + f4(1) + f5(1) + f6(1) + f7(1) + a.m1(1) + a.m2(1) +
         a.m3(1) + b.m1(1) + b.m2(1);
}
