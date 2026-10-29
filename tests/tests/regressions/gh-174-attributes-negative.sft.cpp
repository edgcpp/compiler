//type:fn
//options_all:--c++20 --gnu_version=150000 --diag_error=pragma_must_precede_statement
// GCC rejects a loop pragma that is followed by the attributes of the loop
// rather than by the loop keyword: the pragma must follow the attributes.
void h(int);

void f1(int *a, int n) {
#pragma GCC unroll 4                      // Error: precedes the attributes.
  [[likely]] for (int i = 0; i < n; ++i)
    h(a[i]);
#pragma GCC ivdep                         // Error: precedes the attributes.
  [[unlikely]] while (n-- > 0)
    h(a[n]);
}
