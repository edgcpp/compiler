//type:fn
//options_all:--c++20
//options:--clang --clang_version=180000:--g++ --gnu_version=130300:--clang --clang_version=60000:--g++ --gnu_version=70500
//remark:Unsafe qualification conversions through array levels

using A = int[3];

void take(const A **);
void take2(const int *(*)[3]);
void take3(const double *const (*)[3]);

void reject(A **p, int *(*q)[3], double *const (*r)[3]) {
  // Not const-safe: rejected by GCC and Clang.
  take(p);
  take2(q);
  // Const-safe, and accepted by Clang 7.0 and later and by GCC 10 and later.
  // Earlier versions do not look through the array level, so they reject it.
  take3(r);
}
