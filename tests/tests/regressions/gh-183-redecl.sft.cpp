//type:fp
//options_all:--gnu_version=150000 --diag_error=inline_gnu_noinline_conflict
//options:--c++17
//require:BACK_END_IS_CP_GEN_BE 0
// Like gh-183.sft.cpp: GCC does not diagnose an "inline" definition with the
// "noinline" attribute that follows a declaration without "inline".  (The
// C++-generating back end puts out the earlier declaration with "inline", so
// its generated code would legitimately be diagnosed; hence the requirement.)
#define NOINLINE __attribute__((__noinline__))

int f(int x);
inline NOINLINE int f(int x) { return x; }

struct A {
  int m(int x);
};
inline NOINLINE int A::m(int x) { return x; }

int g() {
  A a;
  return f(1) + a.m(1);
}
