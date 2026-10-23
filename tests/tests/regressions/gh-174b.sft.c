//options_all:--gcc --gnu_version=150000
//type:cp
// In C99 and later, iteration and selection statements are wrapped in an
// implicit block.  The GCC loop pragmas must still be emitted immediately in
// front of the loop they apply to (inside that block), and a loop pragma that
// follows such a statement must not be emitted inside its implicit block.
void h(void);

void f1(int *a, int n) {
  h();
#pragma GCC unroll 4
  for (int i = 0; i < n; ++i)
    a[i] = i;
}

void f2(int *a, int n) {
  int i;
#pragma GCC ivdep
  for (i = 0; i < n; ++i)
    a[i] = i;
#pragma GCC unroll 2
  while (n > 0)
    a[--n] = 0;
}

void f3(int *a, int n) {
  for (int j = 0; j < n; ++j)
#pragma GCC unroll 0
    for (int i = 0; i < n; ++i)
      a[i] += j;
}

void f4(int *a, int n) {
  if (n > 0)
    h();
#pragma GCC novector
  do
    a[--n] = 0;
  while (n > 0);
}
