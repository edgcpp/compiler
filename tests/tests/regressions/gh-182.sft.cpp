//type:cp
//remark:[GH #182] "GCC diagnostic" pragmas were not recognized
//options:--g++ --gnu_version=150000:--g++ --gnu_version=120000:--gcc --gnu_version=150000:--clang
//options_all:--diag_error=unrecognized_gcc_pragma
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
int f(int unused) { return 0; }
#pragma GCC diagnostic pop

#define IGNORE_SHADOW _Pragma("GCC diagnostic ignored \"-Wshadow\"")
void g(int *p) {
  _Pragma("GCC diagnostic push")
  IGNORE_SHADOW
  *p = 0;
  _Pragma("GCC diagnostic pop")
}
