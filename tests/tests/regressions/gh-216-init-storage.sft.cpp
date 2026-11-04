//type: rp
//options: --c++26:--c++26 -tused
//remark:[GH #216] Constexpr storage for expansion init-statement variables.

namespace uninitialized_init_statement {
  constexpr int f() {
    int sum = 0;
    template for (int n; auto x : {1, 2}) {
      n = x;
      sum += n;
    }
    return sum;
  }
  static_assert(f() == 3);
}

namespace repeated_scope_entry {
  constexpr int f() {
    int sum = 0;
    for (int i = 0; i != 3; ++i) {
      template for (int n; auto x : {1, 2}) {
        n = x + i;
        sum += n;
      }
    }
    return sum;
  }
  static_assert(f() == 15);
}

namespace pointer_init_statement {
  struct S { int n; };

  constexpr int f() {
    S s{0};
    template for (int *p; auto x : {1, 2}) {
      p = &s.n;
      *p += x;
    }
    return s.n;
  }
  static_assert(f() == 3);
}

namespace transfer_cleanup {
  constexpr int f() {
    int sum = 0;
    template for (int n; auto x : {1, 2, 3}) {
      n = x;
      if (n == 1) continue;
      sum += n;
      if (n == 2) break;
    }
    return sum;
  }
  static_assert(f() == 2);

  constexpr int g() {
    template for (int n; auto x : {1, 2}) {
      n = x;
      if (n == 1) return n;
    }
    return 0;
  }
  static_assert(g() == 1);
}

namespace instantiated_init_statement {
  template<typename T> constexpr int f() {
    int sum = 0;
    template for (T n; auto x : {1, 2}) {
      n = x;
      sum += n;
    }
    return sum;
  }
  static_assert(f<int>() == 3);
}

int main() {
  return uninitialized_init_statement::f() != 3 ||
         repeated_scope_entry::f() != 15 ||
         pointer_init_statement::f() != 3 || transfer_cleanup::f() != 2 ||
         transfer_cleanup::g() != 1 ||
         instantiated_init_statement::f<int>() != 3;
}
