//type:fn
//options_all:--c++20
//options:--g++ --gnu_version=130300:--g++ --gnu_version=150200:--clang --clang_version=180000:--strict
//remark:Unsafe multilevel pointer qualification conversions

// Adding const below the first level without adding it at every level in
// between is not const-safe; GCC and Clang reject these conversions.

void take(const char **);
void take3(const char ***);

void reject(char **p, char ***q) {
  take(p);
  const char **bad = p;
  take(bad);
  take3(q);
  take(static_cast<const char **>(p));
}
