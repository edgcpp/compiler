//type:fp
//options_all:--c++26
//remark:P3068R6: destruction of loop variables when an exception is thrown

namespace lifetimes {
  // Condition variables live until the end of the iteration, and the
  // variable of a range-based for-statement is destroyed at the end of each
  // iteration, including when an exception exits the loop.
  struct S {
    int* p;
    int* cnt;
    constexpr S(int* p, int* cnt) : p(p), cnt(cnt) { *p = *p * 10 + 1; }
    constexpr S(const S& s) : p(s.p), cnt(s.cnt) { *p = *p * 10 + 1; }
    constexpr ~S() { *p = *p * 10 + 2; }
    constexpr explicit operator bool() const { return (*cnt)-- > 0; }
  };

  constexpr int while_throw() {
    int n = 0, cnt = 3;
    try {
      while (S s{&n, &cnt}) { n = n * 10 + 3; if (cnt == 1) throw 0; }
    } catch (int) { n = n * 10 + 5; }
    return n;
  }
  static_assert(while_throw() == 1321325);

  constexpr int range_for_throw() {
    int n = 0, cnt = 0;
    S a[2] = {S(&n, &cnt), S(&n, &cnt)};
    n = 0;
    try {
      for (S s : a) { n = n * 10 + 3; throw 0; }
    } catch (int) { n = n * 10 + 5; }
    return n;
  }
  static_assert(range_for_throw() == 1325);
}
