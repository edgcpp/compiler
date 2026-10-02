//options_all:--c++17 --gnu_version=150000
//type:cp
// The GCC loop pragmas must be emitted immediately in front of the loop they
// apply to; previously they were emitted at the start of the enclosing block,
// which the back-end compiler rejects.
void h();

void f1(char *first, char *last) {
  h();
#pragma GCC unroll 4
  while (first != last)
    ++first;
}

struct It { char *p; };
bool operator!=(It a, It b);

void f2(It first, It last) {
#pragma GCC unroll 4
  while (first != last)
    ++first.p;
}

void f3(int *a, int n) {
  h();
#pragma GCC unroll 4
#pragma GCC ivdep
  for (int i = 0; i < n; ++i)
    a[i] = i;
}

struct R { int *begin(); int *end(); };

void f4(R r) {
  h();
#pragma GCC unroll 2
  for (int &x : r)
    x = 0;
}

void f5(int *a, int n) {
  h();
#pragma GCC novector
  do
    a[--n] = 0;
  while (n > 0);
}
