//type:fp
//options:--c++11:--c++17:--gn 150200 --c++11:--gn 150200 --c++17:--clang_version 220100 --c++11:--clang_version 220100 --c++17
//remark:Type-transforming intrinsics preserve qualified function types.

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

// Exercise substitution as well as direct uses of the intrinsics.
template<typename T, typename LVALUE_REF = T, typename RVALUE_REF = T,
         typename POINTER = T, typename DECAY = T>
struct Tester
{
  static_assert(is_same<__add_lvalue_reference(T), LVALUE_REF>::value,
                "lvalue reference");
  static_assert(is_same<__add_rvalue_reference(T), RVALUE_REF>::value,
                "rvalue reference");
  static_assert(is_same<__add_pointer(T), POINTER>::value, "pointer");
  static_assert(is_same<__decay(T), DECAY>::value, "decay");
};

#define TEST(QUAL)                                     \
  static_assert(sizeof(Tester<void() QUAL>) != 0, ""); \
  static_assert(sizeof(Tester<void(int, ...) QUAL>) != 0, "");

namespace qualified_function_types
{
  TEST(const)
  TEST(volatile)
  TEST(const volatile)
  TEST(&)
  TEST(const &)
  TEST(volatile &)
  TEST(const volatile &)
  TEST(&&)
  TEST(const &&)
  TEST(volatile &&)
  TEST(const volatile &&)
}

#if __cplusplus >= 201703L
namespace qualified_noexcept_function_types
{
  TEST(const noexcept)
  TEST(volatile noexcept)
  TEST(const volatile noexcept)
  TEST(& noexcept)
  TEST(const & noexcept)
  TEST(volatile & noexcept)
  TEST(const volatile & noexcept)
  TEST(&& noexcept)
  TEST(const && noexcept)
  TEST(volatile && noexcept)
  TEST(const volatile && noexcept)
}
#endif

#undef TEST

namespace alias_operands
{
  using F = void() const;
  using G = void() &;
  static_assert(is_same<__add_lvalue_reference(F), F>::value, "");
  static_assert(is_same<__add_rvalue_reference(G), G>::value, "");
  static_assert(is_same<__add_pointer(F), F>::value, "");
  static_assert(is_same<__decay(G), G>::value, "");
}

namespace object_and_reference_types
{
  Tester<int, int&, int&&, int*, int> unqualified;
  Tester<const int, const int&, const int&&, const int*, int> qualified;
  Tester<const int&, const int&, const int&, const int*, int> lvalue;
  Tester<int&&, int&, int&&, int*, int> rvalue;
}

namespace void_types
{
  Tester<void, void, void, void*, void> unqualified;
  using V = const volatile void;
  Tester<V, V, V, V*, void> qualified;
}

namespace array_types
{
  Tester<int[3], int(&)[3], int(&&)[3], int(*)[3], int*> known_bound;
  Tester<int[], int(&)[], int(&&)[], int(*)[], int*> unknown_bound;
}

namespace unqualified_function_types
{
  using H = void();
  Tester<H, H&, H&&, H*, H*> function;
  Tester<H&, H&, H&, H*, H*> lvalue;
  Tester<H&&, H&, H&&, H*, H*> rvalue;
}

#if __cplusplus >= 201703L
namespace unqualified_noexcept_function_types
{
  using N = void() noexcept;
  Tester<N, N&, N&&, N*, N*> function;
}
#endif
