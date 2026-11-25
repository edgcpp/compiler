//type:fn
//options_all:-tused
//options:--c++11 -A:--c++11 --gnu_version 150200:--c++11 --clang_version 220100:--ms_c++17 --microsoft_version 1950
//remark:[GH #592] An unrelated std cannot supply missing range bounds.

namespace range_adl {
  struct Range {};
  int *begin(Range&);
}

namespace std {
  int *end(range_adl::Range&);
}

namespace range_adl {
  template<typename T> void iterate(T& r) {
    for (int i : r) (void)i;
  }

  void test(Range& r) { iterate(r); }
}

/* Changes example: Neither bound is found by ADL. */
namespace N { struct S {}; }
namespace std { int *begin(N::S&); int *end(N::S&); }
void f(N::S& s) {
  for (int i : s) { }
}
