//type: fp
//options: --c++26 --microsoft_version 1952 --ms_permissive
//remark:[GH #216] Preserve statements following __if_exists bodies.

namespace cached_if_exists {
  constexpr int f() {
    int n = 0;
    template for (auto x : {1})
      if (true) __if_exists(n) { ++n; }
    ++n;
    return n;
  }
  static_assert(f() == 2);
}

namespace nested_cached_if_exists {
  constexpr int f() {
    int n = 0;
    template for (auto x : {1})
      if (true) __if_exists(n) { { ++n; } }
    ++n;
    return n;
  }
  static_assert(f() == 2);
}

namespace cached_if_not_exists {
  struct S {};
  constexpr int f() {
    int n = 0;
    template for (auto x : {1})
      if (true) __if_not_exists(S::x) { ++n; }
    ++n;
    return n;
  }
  static_assert(f() == 2);
}

namespace discarded_if_exists {
  constexpr int f() {
    int n = 0;
    if constexpr (false)
      if (true) __if_exists(n) { ++n; }
    ++n;
    return n;
  }
  static_assert(f() == 1);
}

namespace discarded_if_not_exists {
  struct S {};
  constexpr int f() {
    int n = 0;
    if constexpr (false)
      if (true) __if_not_exists(S::x) { ++n; }
    ++n;
    return n;
  }
  static_assert(f() == 1);
}
