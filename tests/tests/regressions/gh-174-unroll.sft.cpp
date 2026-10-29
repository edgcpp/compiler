//options_all:--c++17 --gnu_version=150000 -tused
//type:cp
// The argument of "#pragma GCC unroll" is evaluated by the front end and its
// value is emitted in the generated code.  Previously the argument was copied
// unchanged, so C++ constructs and template parameters ended up in the
// generated C code, which the back-end compiler rejects.
void h(int);

constexpr int n = 3;
enum { E = 5 };

void f1(int *a) {
#pragma GCC unroll sizeof(decltype(0))
  for (int i = 0; i < 4; ++i)
    h(a[i]);
#pragma GCC unroll(n)
  for (int i = 0; i < 4; ++i)
    h(a[i]);
#pragma GCC unroll E
  while (*a)
    ++a;
}

template <int N>
void f2(int *a) {
#pragma GCC unroll(N)
  for (int i = 0; i != N; ++i)
    h(a[i]);
#pragma GCC unroll N
  for (int i = 0; i != N; ++i)
    h(a[i]);
}

struct S {
  template <int N>
  void f3(int *a) {
#pragma GCC unroll(N)
    for (int i = 0; i != N; ++i)
      h(a[i]);
  }
};

void f4(int *a) {
  S s;
  f2<6>(a);
  f2<10>(a);
  s.f3<2>(a);
}
