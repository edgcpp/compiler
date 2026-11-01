//type:fn
//options_all:--c++20
//options:--g++ --gnu_version=130300:--g++ --gnu_version=150200:--strict
void reject(char **p, char ***q) {
  const char **bad = p;
  const char ***also_bad = q;
  (void)bad;
  (void)also_bad;
}
