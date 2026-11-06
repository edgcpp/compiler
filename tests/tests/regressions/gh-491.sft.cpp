//type:fp
//options:--c++14:--c++17:--c++14 --gn 150200:--c++14 --clang_version 220100:--ms_c++20 --microsoft_version 1950
//options_all:-tused
//remark:Recorded packs in generic lambdas

namespace call_expansion {
  int g(int) noexcept;
  int g(int, int) noexcept;

  template<typename... Ts>
  auto f(Ts... x) noexcept {
    return [](auto) noexcept(noexcept(g(x...))) { return 0; };
  }
  static_assert(noexcept(f(42)(0)), "");
  static_assert(noexcept(f(1, 2)(0)), "");
}

namespace sizeof_pack {
  template<typename... Ts>
  auto f(Ts... x) noexcept {
    return [](auto) noexcept(sizeof...(x) != 0) { return 0; };
  }
  static_assert(noexcept(f(42)(0)), "");
  static_assert(noexcept(f(1, 2)(0)), "");
}

namespace default_argument {
#if __cpp_constexpr >= 201603L
  constexpr
#endif
  int f() {
    auto g = [](auto... x) {
      return [](auto, int n = sizeof...(x)) { return n; }(0);
    };
    return g(1, 2);
  }
#if __cpp_constexpr >= 201603L
  static_assert(f() == 2, "");
#endif
}

namespace enclosing_pack_restoration {
  void g(...) noexcept;

  template<typename F>
  int h(F f) {
    static_assert(noexcept(f(0)), "");
    return 0;
  }

  template<typename... Ts>
  void f(Ts... x) {
    int a[] = {0, h([](auto) noexcept(noexcept(g(x...)) &&
                                     sizeof(Ts) == sizeof(int)) {
      return 0;
    })...};
  }

  void test() {
    f(1);
    f(1, 2);
  }
}

#if __cpp_fold_expressions >= 201603L
namespace fold_expression {
  template<typename... Ts>
  auto f(Ts... x) noexcept {
    return [](auto) noexcept((noexcept(x) && ...)) { return 0; };
  }
  static_assert(noexcept(f(42)(0)), "");
  static_assert(noexcept(f(1, 2)(0)), "");
}
#endif
