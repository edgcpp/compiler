//type:fp
//options_all:--c++20
//options:--g++ --gnu_version=130300:--clang --clang_version=180000:--strict
//remark:Const-safe multilevel pointer qualification conversions

void take(char *const *);
void take_c(const char *const *);
void take3(const char *const *const *);

void allow(char **p, char ***q) {
  take(p);
  take_c(p);
  take3(q);
}
