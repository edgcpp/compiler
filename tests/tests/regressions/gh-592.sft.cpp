//type:fp
//options:--c++11 -A:--c++11 --gnu_version 150200:--c++11 --clang_version 220100:--ms_c++17 --microsoft_version 1950
//remark:[GH #592] An unrelated std overload cannot hide ADL bounds.

namespace range_adl {
  struct Range {};
  int *begin(const Range&);
  int *end(const Range&);
}

namespace std {
  int *begin(range_adl::Range&) = delete;
  int *end(range_adl::Range&) = delete;
}

namespace range_adl {
  void test(Range& r) {
    for (int i : r) (void)i;
  }
}
