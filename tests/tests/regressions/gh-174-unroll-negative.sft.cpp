//type:fn
//options_all:--c++17 --gnu_version=150000 -tused
// Arguments of "#pragma GCC unroll" that GCC rejects: the argument must be an
// integral constant expression with a value in the range 0 to 65534.
void h(int);

void f1(int *a, int v) {
#pragma GCC unroll 1.0f                   // Error: not integral.
  for (int i = 0; i < 4; ++i)
    h(a[i]);
#pragma GCC unroll -42                    // Error: negative.
  for (int i = 0; i < 4; ++i)
    h(a[i]);
#pragma GCC unroll 65535                  // Error: too large.
  for (int i = 0; i < 4; ++i)
    h(a[i]);
#pragma GCC unroll 0xffffffffffffffffULL  // Error: too large.
  for (int i = 0; i < 4; ++i)
    h(a[i]);
#pragma GCC unroll v                      // Error: not a constant.
  for (int i = 0; i < 4; ++i)
    h(a[i]);
}

template <int N>
void f2(int *a) {
#pragma GCC unroll(N - 42)                // Error in f2<1>: negative.
  for (int i = 0; i < 4; ++i)
    h(a[i]);
}

void f3(int *a) {
  f2<1>(a);
}
