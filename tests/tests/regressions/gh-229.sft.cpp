//options::--gnu_version 130300
//options_all:--c++17

template <bool B> constexpr int h() {
  return [] { if constexpr (B) while (false) {} return 1; return 2; }();
}

static_assert(h<false>() == 1, "h<false>() must be 1");

// Nested unbraced substatements: only one statement may be discarded.
template <bool B> constexpr int nested() {
  return [] {
    if constexpr (B) while (B) while (!B) { };
    return 1;
  }();
}
static_assert(nested<false>() == 1, "nested");

int arr[2];

template <bool B> constexpr int others() {
  return [] {
    int n = 0;
    if constexpr (B) for (;;) {}
    n += 1;
    if constexpr (B) for (int i : arr) for (int j : arr) { }
    n += 1;
#ifdef __cpp_expansion_statements
    if constexpr (B) template for (auto x : arr) { }
#endif
    n += 1;
    if constexpr (B) do {} while (0);
    n += 1;
    if constexpr (B) switch (n) { case 1: break; }
    n += 1;
    if constexpr (B) try {} catch (int) {} catch (...) {}
    n += 1;
    if constexpr (B) L: while (B) { }
    n += 1;
    if constexpr (B) x: if (B) while (B) {} else for (;;) {}
    n += 1;
    if constexpr (B) [] {}();
    n += 1;
    if constexpr (B) ; else while (B) {}
    n += 1;
    // Expression statements containing lambdas: everything after the
    // lambda's closing brace must also be discarded.
    if constexpr (B) n = [] { return 100; }() + 1000, n = 999999;
    n += 1;
    if constexpr (B) n = [&] { if (n) { n = 5; } return n; }() * 7 +
                         [] { return 1; }() + arr[0], n = 999999;
    n += 1;
    if constexpr (B) auto l = [] { return 1; }, m = [] { return 2; },
                     o = (n = 999999);
    n += 1;
    if constexpr (!B) n += 1; else n = [] { return 100; }() + 1000,
                                   n = 999999;
    if constexpr (B) n = 100; else while (B) {}
    return n;
  }();
}
static_assert(others<false>() == 14, "others");
