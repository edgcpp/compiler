//type:fp
//options:--c++17 -A:--c++17 --gnu_version 150200:--c++17 --clang_version 220100:--ms_c++17 --microsoft_version 1950
//remark:[GH #592] Tuple-like structured bindings use only ADL for get.

namespace binding_adl {
  struct Tuple {};
  template<decltype(sizeof(0)) I> int& get(const Tuple&);
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

  /* This better match must not be considered: std is not associated. */
  template<decltype(sizeof(0)) I> int& get(binding_adl::Tuple&) = delete;

  struct Tuple {};
  template<> struct tuple_size<Tuple> {
    static constexpr decltype(sizeof(0)) value = 1;
  };
  template<> struct tuple_element<0, Tuple> {
    using type = int;
  };
  template<decltype(sizeof(0)) I> int& get(Tuple&);
}

namespace binding_ordinary_lookup_ignored {
  template<decltype(sizeof(0)) I> int& get(binding_adl::Tuple&) = delete;

  void test(binding_adl::Tuple& t) {
    auto& [value] = t;
    (void)value;
  }
}

namespace binding_associated_std {
  void test(std::Tuple& t) {
    auto& [value] = t;
    (void)value;
  }
}
