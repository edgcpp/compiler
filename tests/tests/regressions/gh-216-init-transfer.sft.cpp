//type: fp
//options: --c++26 --gnu_version 150200
//remark:[GH #216] Transfers out of an expansion init-statement.

namespace outer_loop_break {
  constexpr int f() {
    int n = 0;
    for (int i = 0; i != 3; ++i) {
      template for (({ break; }); auto x : {1, 2}) {
        n += x;
      }
      ++n;
    }
    return n;
  }
  static_assert(f() == 0);
}

namespace outer_loop_continue {
  constexpr int f() {
    int n = 0;
    for (int i = 0; i != 3; ++i) {
      template for (({ continue; }); auto x : {1, 2}) {
        n += x;
      }
      ++n;
    }
    return n;
  }
  static_assert(f() == 0);
}
