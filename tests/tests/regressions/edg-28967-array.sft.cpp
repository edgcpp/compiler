//type:fp
//options_all:--c++20
//options:--clang --clang_version=180000:--clang --clang_version=70000:--g++ --gnu_version=130300:--microsoft --microsoft_version=1910:--microsoft --microsoft_version=1913:--microsoft --microsoft_version=1914:--microsoft --microsoft_version=1940
//remark:Const-safe qualification conversions through array levels

// Clang 7.0 and later, GCC 10 and later, and MSVC 19.14 and later look through
// array levels when checking a qualification conversion (N4261, Core issue 330).

void take(const double *const (*)[3]);
void take2(char *const (*)[2]);

void allow(double *const (*p)[3], char *(*q)[2]) {
  take(p);
  take2(q);
}
