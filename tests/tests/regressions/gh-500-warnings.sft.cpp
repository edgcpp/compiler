//type:fp
//options:--c++20:--c++20 --gnu_version 150200:--c++20 --clang_version 220100:--ms_c++20 --microsoft_version 1950
//options_all:-tused

namespace suppressed_in_prototype
{
  template<int... V> void f() {
    []<int... S>() { ((S * V) * ... * 1); };
  }

  template void f<>();
}

namespace real_instantiation
{
  template<int... V> void f() {
    auto l = []<int... S>() { ((S * V) * ... * 1); };
    l.template operator()<3>();
  }

  template void f<2>();
}
