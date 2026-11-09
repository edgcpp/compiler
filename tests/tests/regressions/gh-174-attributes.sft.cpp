//options_all:--c++20 --gnu_version=150000 -tused --diag_error=pragma_must_precede_statement
//type:cp
// GCC requires a loop pragma to immediately precede the loop keyword, so in
// the generated code the pragma must follow any attributes of the loop.
// Previously it was emitted in front of the attributes, which the back-end
// compiler rejects ("for, while or do statement expected").  The --diag_error
// makes the front end reject that order too when it compiles the generated
// code again (as the C++-generating configurations do).
void h(int);

void f1(int *a, int n) {
  [[likely]]
#pragma GCC unroll 4
  for (int i = 0; i < n; ++i)
    h(a[i]);
}

void f2(int *a, int n) {
  [[unlikely]]
#pragma GCC ivdep
  while (n-- > 0)
    h(a[n]);
}

void f3(int *a, int n) {
  [[likely]]
#pragma GCC novector
  do
    h(a[--n]);
  while (n > 0);
}

void f4(int *a, int n) {
  [[likely]]
#pragma GCC unroll 2
#pragma GCC ivdep
  for (int i = 0; i < n; ++i)
    a[i] = i;
}

struct R { int *begin(); int *end(); };

void f5(R r) {
  [[likely]]
#pragma GCC unroll 2
  for (int &x : r)
    x = 0;
}

template <int N>
void f6(int *a) {
  [[likely]]
#pragma GCC unroll(N)
  for (int i = 0; i != N; ++i)
    h(a[i]);
}

void f7(int *a) {
  f6<3>(a);
}
