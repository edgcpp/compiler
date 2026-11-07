//type:fp
//options_all:--c++20
//options:--g++ --gnu_version=130300:--strict
void allow(char **p, char ***q) {
  char *const *a = p;
  const char *const *b = p;
  const char *const *const *c = q;
  (void)a; (void)b; (void)c;
}
