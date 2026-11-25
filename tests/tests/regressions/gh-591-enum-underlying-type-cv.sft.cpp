//type:fp
//options:--c++14:--g++ --gnu_version 110200 --c++14:--clang_version 220100 --c++14:--ms_c++20 --microsoft_version 1950
//remark:Ignore cv-qualification when determining an enum underlying type.

template<typename, typename>
struct is_same
{
  static constexpr bool value = false;
};

template<typename T>
struct is_same<T, T>
{
  static constexpr bool value = true;
};

namespace direct_qualifiers
{
  enum class C : const unsigned {};
  static_assert(is_same<__underlying_type(C), unsigned>::value, "const");

  enum class V : volatile unsigned {};
  static_assert(is_same<__underlying_type(V), unsigned>::value, "volatile");

  enum class CV : const volatile unsigned {};
  static_assert(is_same<__underlying_type(CV), unsigned>::value,
                "const volatile");
}

namespace typedef_qualifiers
{
  using T = const unsigned;
  enum E : T {};
  static_assert(is_same<__underlying_type(E), unsigned>::value, "typedef");

  using U = volatile T;
  enum F : U {};
  static_assert(is_same<__underlying_type(F), unsigned>::value,
                "qualified typedef");

  enum G : volatile T {};
  static_assert(is_same<__underlying_type(G), unsigned>::value,
                "qualified typedef use");

  using V = volatile unsigned;
  enum H : const V {};
  static_assert(is_same<__underlying_type(H), unsigned>::value,
                "mixed qualified typedef");
}

namespace typedef_fixed_base_promotion
{
  using T = const unsigned char;
  enum E : volatile T { e };

  char f(unsigned char);
  int f(int);
  static_assert(is_same<decltype(f(e)), char>::value,
                "typedef fixed base promotion");
}

namespace typedef_enumerator_type
{
  using T = const unsigned;
  enum class E : volatile T
  {
    first = 0,
    second = is_same<decltype(first), unsigned>::value ? 1 : 0
  };
  static_assert(E::second != E::first, "qualified typedef enumerator type");
}

namespace dependent_qualifiers
{
  template<typename T>
  struct S
  {
    enum class E : T {};
  };

  static_assert(
      is_same<__underlying_type(S<const unsigned>::E), unsigned>::value,
      "dependent type");

  template<typename T>
  struct U
  {
    enum class E : T
    {
      first = 0,
      second = is_same<decltype(E::first), unsigned>::value ? 1 : 0
    };
  };

  static_assert(U<const unsigned>::E::second != U<const unsigned>::E::first,
                "dependent enumerator type");

  template<typename T>
  struct V
  {
    enum class E : const T
    {
      first = 0,
      second = is_same<decltype(E::first), unsigned>::value ? 1 : 0
    };
  };

  static_assert(is_same<__underlying_type(V<unsigned>::E), unsigned>::value,
                "const dependent type");
  static_assert(V<unsigned>::E::second != V<unsigned>::E::first,
                "const dependent enumerator type");
}

namespace fixed_base_promotion
{
  enum E : const volatile unsigned char { e };

  char f(unsigned char);
  int f(int);
  static_assert(is_same<decltype(f(e)), char>::value,
                "fixed base promotion");
}

namespace enumerator_type
{
  enum class E : const unsigned {
    first = 0,
    second = is_same<decltype(first), unsigned>::value ? 1 : 0
  };
  static_assert(E::second != E::first, "unqualified enumerator type");
}

namespace minimal
{
  enum E : const unsigned char { e };
  void f(unsigned char);
  void f(int) = delete;
  void g() { f(e); }
}
