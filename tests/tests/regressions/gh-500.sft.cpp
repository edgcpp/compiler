//type:fp
//options:--c++20:--c++20 --gn 150200:--c++20 --clang_version 220100:--ms_c++20 --microsoft_version 1950
//options_all:--il_display -tused
//filter:grep -E '(tk_error|enk_error)'

namespace minimal
{
  template<int... Is> void f() {
    []<int... Js>() { ((Js * Is) * ... * 1); };
  }
  template void f<>();
}

namespace inner_pack_first
{
  template<int... V> constexpr auto f() {
    return []<int... S>() { return ((S * V) * ... * 1); };
  }

  static_assert(f<>().template operator()<>() == 1);
  static_assert(f<2>().template operator()<3>() == 6);
  static_assert(f<2, 3>().template operator()<4, 5>() == 120);
}

namespace outer_pack_first
{
  template<int... V> constexpr auto f() {
    return []<int... S>() { return ((V * S) * ... * 1); };
  }

  static_assert(f<>().template operator()<>() == 1);
  static_assert(f<2>().template operator()<3>() == 6);
  static_assert(f<2, 3>().template operator()<4, 5>() == 120);
}

namespace uninstantiated_lambda
{
  template<int... V> void f() {
    []<int... S>() { (void)((S * V) * ... * 1); };
  }

  void g() { f<>(); }
}
