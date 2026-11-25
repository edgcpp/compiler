//type:fn
//options:--c++17 -A:--c++17 --gnu_version 150200:--c++17 --clang_version 220100:--ms_c++17 --microsoft_version 1950
//remark:[GH #592] Unrelated std cannot supply get for a structured binding.

namespace binding_adl {
  struct Tuple {};
}

namespace std {
  template<typename T> struct tuple_size;
  template<decltype(sizeof(0)) I, typename T> struct tuple_element;

  template<> struct tuple_size<binding_adl::Tuple> {
    static constexpr decltype(sizeof(0)) value = 1;
  };
  template<> struct tuple_element<0, binding_adl::Tuple> {
    using type = int;
  };

  template<decltype(sizeof(0)) I> int& get(binding_adl::Tuple&);
}

namespace binding_adl {
  void test(Tuple& t) {
    auto& [value] = t;
    (void)value;
  }
}
