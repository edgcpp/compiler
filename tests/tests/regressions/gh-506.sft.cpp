//type:fp
//options:--c++20:--c++20 --gn 150200:--c++20 --clang_version 220100:--ms_c++20 --microsoft_version 1950
//options_all:-tused

namespace nontype_template_parameter_default
{
  template<int... Is> constexpr auto f() {
    return [](auto) {
      return []<int... Js, int N = ((Js * Is) * ... * 1)>() {
        return N;
      };
    };
  }

  static_assert(f<>()(0).template operator()<>() == 1);
  static_assert(f<2, 3>()(0).template operator()<4, 5>() == 120);
}

namespace uninstantiated_template_parameter_default
{
  template<int... Is> void f() {
    [](auto) {
      []<int... Js, typename U = int[((Js * Is) * ... * 1) - 2]>() {};
    };
  }
  template void f<2>();
}

namespace inner_pack_first_in_template_parameter_default
{
  template<int... Is> constexpr auto f() {
    return [](auto) {
      return []<int... Js,
                typename U = int[((Js * Is) * ... * 1) - 2]>() {
        return sizeof(U);
      };
    };
  }

  static_assert(f<2>()(0).template operator()<3>() == 4 * sizeof(int));
  static_assert(f<2, 3>()(0).template operator()<4, 5>() == 118 * sizeof(int));
}

namespace outer_pack_first_in_template_parameter_default
{
  template<int... Is> constexpr auto f() {
    return [](auto) {
      return []<int... Js,
                typename U = int[((Is * Js) * ... * 1) - 2]>() {
        return sizeof(U);
      };
    };
  }

  static_assert(f<2>()(0).template operator()<3>() == 4 * sizeof(int));
  static_assert(f<2, 3>()(0).template operator()<4, 5>() == 118 * sizeof(int));
}
