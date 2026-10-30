//type:fp
//options:--c++20:--c++20 --gnu_version=150200:--c++20 --clang_version=210000
//options_all:-w
//remark:[GH #338] Lambdas in template signatures can name enclosing template parameters.

namespace test_support
{
  template<class, class> constexpr bool same_type = false;
  template<class T> constexpr bool same_type<T, T> = true;
}

// Rescanning a lambda during substitution must use the template arguments of
// the current instance, and restore the template parameters afterwards.
namespace lambda_parameter_type
{
  template<class T>
  constexpr void f(char (*)[sizeof([](T x) { return x; }(T{}))]) {}

  constexpr bool test()
  {
    char a[sizeof(char)]{}, b[sizeof(int)]{}, c[sizeof(long)]{};
    f<char>(&a);
    f<int>(&b);
    f<long>(&c);
    f<char>(&a);
    return true;
  }
  static_assert(test());
}

namespace unnamed_lambda_parameter
{
  template<class T>
  constexpr void f(char (*)[sizeof([](T) { return T(); }(1))]) {}

  constexpr bool test()
  {
    char a[sizeof(int)]{}, b[sizeof(long long)]{};
    f<int>(&a);
    f<long long>(&b);
    return true;
  }
  static_assert(test());
}

namespace template_parameter_in_body_only
{
  template<class T>
  constexpr void f(char (*)[sizeof([](int) { return T(); }(1))]) {}

  constexpr bool test()
  {
    char a[sizeof(int)]{}, b[sizeof(long long)]{};
    f<int>(&a);
    f<long long>(&b);
    return true;
  }
  static_assert(test());
}

namespace generic_lambda
{
  template<class T>
  constexpr void f(char (*)[sizeof([](auto x) { return T(x); }(1))]) {}

  template<class T>
  constexpr void g(char (*)[sizeof([](auto) { return T(); }(1))]) {}

  constexpr bool test()
  {
    char a[sizeof(int)]{}, b[sizeof(long long)]{};
    f<int>(&a);
    f<long long>(&b);
    g<int>(&a);
    g<long long>(&b);
    return true;
  }
  static_assert(test());
}

namespace trailing_return_type
{
  template<class T>
  constexpr auto f(T x) -> decltype([](T y) { return y; }(x)) { return x; }

  static_assert(test_support::same_type<decltype(f(1)), int>);
  static_assert(test_support::same_type<decltype(f('a')), char>);

  static_assert(f(1) == 1);
  static_assert(f(2) == 2);
}

namespace requires_clause
{
  template<class T>
    requires (sizeof([] { return T{}; }()) == sizeof(int))
  constexpr void f(T) {}

  constexpr bool test()
  {
    f(1);
    f(2u);
    return true;
  }
  static_assert(test());
}

namespace default_template_argument
{
  template<class T, class U = decltype([] { return T{}; }())>
  constexpr U f() { return U{}; }

  static_assert(test_support::same_type<decltype(f<int>()), int>);
  static_assert(test_support::same_type<decltype(f<char>()), char>);

  static_assert(f<int>() == 0);
  static_assert(f<char>() == '\0');
}

namespace exception_specification
{
  template<class T>
  constexpr void f(T) noexcept(noexcept([] { return T{}; }())) {}

  constexpr bool test()
  {
    f(1);
    f('a');
    return true;
  }
  static_assert(test());
}

namespace recursive_constraint_substitution
{
  // Checking f<int> finds f<long long> by ADL in the constraint.  Use long
  // long rather than long, whose size might make its constraint recursive.
  template<class T> struct W {};

  template<class T>
    requires (sizeof([] { static_assert(sizeof(T) >= 4); return T{}; }()) >= 4)
          && (sizeof(T) > 4 || requires { f(W<decltype(T{} + 1LL)>{}); })
  constexpr int f(W<T>) { return sizeof(T); }

  static_assert(f(W<int>{}) == sizeof(int));
  static_assert(f(W<long long>{}) == sizeof(long long));
}

namespace recursive_lambda_substitution
{
  // Unlike recursive_constraint_substitution, nested deduction happens inside
  // the lambda.  T must still denote int when scanning the following assertion.
  // The long long specialization short-circuits without entering the lambda.
  template<class T> struct W {};

  template<class T>
    requires (sizeof(T) > sizeof(int) || [] {
      (void)sizeof(f(W<decltype(T{} + 1LL)>{}));
      static_assert(sizeof(T) == sizeof(int));
      return true;
    }())
  constexpr int f(W<T>) { return sizeof(T); }

  static_assert(f(W<int>{}) == sizeof(int));
  static_assert(f(W<long long>{}) == sizeof(long long));
  static_assert(f(W<int>{}) == sizeof(int));
}

namespace nested_template_substitution
{
  // Substituting the outer lambda also substitutes a lambda in g's signature.
  template<class U>
  constexpr auto g(U u) -> decltype([](U v) { return v; }(u)) { return u; }

  template<class T>
  constexpr auto f(T x) -> decltype([] { return g(T{}); }()) { return g(x); }

  static_assert(f(1) == 1);
  static_assert(f(2) == 2);
}

namespace member_template
{
  template<class T> struct C
  {
    template<class U>
    constexpr void f(char (*)[sizeof([] { return T{} + U{}; }())]) {}

    template<class U>
    constexpr void g(char (*)[sizeof([](U x) { return x; }(U{}))]) {}
  };

  constexpr bool test()
  {
    char a[sizeof(int)]{}, b[sizeof(long)]{};
    C<int> c;
    c.f<short>(&a);
    c.f<long>(&b);
    c.g<int>(&a);
    c.g<long>(&b);
    return true;
  }
  static_assert(test());
}

namespace partial_specialization_with_pack
{
  template<int N> struct B {};
  template<class T, class... Args> struct S
  {
    static constexpr int value = 0;
  };

  template<class T, class... Args>
  struct S<T, B<sizeof([] { return T{}; }())>, Args...>
  {
    static constexpr int value = 1;
  };

  static_assert(S<int, B<sizeof(int)>, long, char>::value == 1);
  static_assert(S<int, B<sizeof(int) + 1>, long, char>::value == 0);
}

namespace successive_lambdas
{
  template<class T>
  constexpr void f(char (*)[sizeof([] { return T{}; }())],
                   char (*)[sizeof([](auto x) { return T(x); }(1))]) {}

  constexpr bool test()
  {
    char a[sizeof(int)]{}, b[sizeof(long long)]{};
    f<int>(&a, &a);
    f<long long>(&b, &b);
    return true;
  }
  static_assert(test());
}

namespace template_parameter_pack
{
  template<class... Ts>
  constexpr void f(char (*)[([] { return (sizeof(Ts) + ... + 0); }())]) {}

  constexpr bool test()
  {
    char a[sizeof(int) + sizeof(long long)]{};
    f<int, long long>(&a);
    return true;
  }
  static_assert(test());
}

namespace successive_lambdas_with_pack
{
  template<class T, class... Ts>
  constexpr void f(char (*)[sizeof([] { return T{}; }())],
                   char (*)[([] { return sizeof(T) + (sizeof(Ts) + ... + 0); }())]) {}

  constexpr bool test()
  {
    char a[sizeof(int)]{}, b[sizeof(int) + sizeof(long long)]{};
    f<int, long long>(&a, &b);
    return true;
  }
  static_assert(test());
}

namespace label_name
{
  // U is a label, not a reference to the still-undeduced template parameter.
  // Discarding the first overload would silently select the ellipsis overload.
  template<int N, class U>
  constexpr char f(char (*)[sizeof([] { U: return 1; }())], U) { return 0; }
  template<int N> constexpr long f(char (*)[sizeof(int)], ...) { return 1; }

  char a[sizeof(int)]{};
  static_assert(sizeof(f<1>(&a, 1.0)) == sizeof(char));
  static_assert(f<1>(&a, 1.0) == 0);
}

namespace attribute_name
{
  // An attribute token spelled U likewise does not name the template parameter.
  // Suppress its expected warning, but retain the overload-selection checks.
  template<int N, class U>
  constexpr char f(char (*)[sizeof([] { [[U]]; return 1; }())], U) { return 0; }
  template<int N> constexpr long f(char (*)[sizeof(int)], ...) { return 1; }

  char a[sizeof(int)]{};
  static_assert(sizeof(f<1>(&a, 1.0)) == sizeof(char));
  static_assert(f<1>(&a, 1.0) == 0);
}

namespace partially_explicit_template_arguments
{
  // Substituting N must leave the lambda dependent until U is deduced from
  // the second argument, then complete the bound with that deduced type.
  template<int N, class U>
  constexpr void f(char (*)[N + sizeof([] { return U{}; }())], U) {}

  constexpr bool test()
  {
    char a[1 + sizeof(double)]{}, b[1 + sizeof(char)]{};
    f<1>(&a, 1.0);
    f<1>(&b, 'a');
    f<1>(&a, 2.0);
    return true;
  }
  static_assert(test());
}

#if defined(__GNUC__) || defined(__clang__)
// These cases require compatibility mode to fold the lambda call when
// checking the substituted function type.
namespace non_type_template_argument
{
  template<int N> struct A {};
  template<int N> constexpr void f(A<[] { return N; }()>) {}

  constexpr bool test()
  {
    f<1>({});
    f<2>({});
    f<1>(A<1>{});
    f<2>(A<2>{});
    return true;
  }
  static_assert(test());
}

namespace nested_lambda_array_bound
{
  template<int N, class T>
  constexpr void f(char (*)[([] { return [] { return N + int(sizeof(T)); }(); }())]) {}

  constexpr bool test()
  {
    char a[1 + sizeof(int)]{}, b[1 + sizeof(long)]{}, c[4 + sizeof(long)]{};
    f<1, int>(&a);
    f<1, long>(&b);
    f<4, long>(&c);
    return true;
  }
  static_assert(test());
}

namespace undeduced_parameter_overload
{
  // An undeduced U must not resolve to the enclosing U and make the first
  // overload viable before U is deduced as double.
  struct U {};
  template<int N, class U>
  constexpr char f(char (*)[([] { return N + sizeof(U); }())], U) { return 0; }
  template<int N> constexpr long f(char (*)[7], ...) { return 1; }

  char a[7]{};
  static_assert(sizeof(f<6>(&a, 1.0)) == sizeof(long));
  static_assert(f<6>(&a, 1.0) == 1);
}

namespace qualified_type_name
{
  // A qualified name spelled like an undeduced parameter is not that
  // parameter, and must not prevent substitution of the lambda.
  struct T { char c[3]; };
  template<int N, class T>
  constexpr char f(char (*)[sizeof([] { return ::qualified_type_name::T{}; }())], T)
  { return 0; }
  template<int N> constexpr long f(char (*)[3], ...) { return 1; }

  char a[3]{};
  static_assert(sizeof(f<6>(&a, 1.0)) == sizeof(char));
  static_assert(f<6>(&a, 1.0) == 0);
}

namespace data_member_name
{
  struct G { int T; };
  template<int N, class T>
  constexpr void f(char (*)[sizeof([] { return G{}.T; }())], T) {}

  constexpr bool test()
  {
    char a[sizeof(int)]{};
    f<6>(&a, 1.0);
    return true;
  }
  static_assert(test());
}

namespace namespace_member_name
{
  namespace NS { struct U { char c[5]; }; }
  template<int N, class U>
  constexpr void f(char (*)[sizeof([] { return NS::U{}; }())], U) {}

  constexpr bool test()
  {
    char a[5]{};
    f<6>(&a, 1.0);
    return true;
  }
  static_assert(test());
}

namespace qualified_member_template_name
{
  struct G { template<class> static constexpr int T = 3; };
  template<int N, class T>
  constexpr char f(char (*)[([] { return G::template T<int>; }())], T) { return 0; }
  template<int N> constexpr long f(char (*)[3], ...) { return 1; }

  char a[3]{};
  static_assert(sizeof(f<6>(&a, 1.0)) == sizeof(char));
  static_assert(f<6>(&a, 1.0) == 0);
}

namespace object_member_template_name
{
  struct G { template<class> static constexpr int T() { return 3; } };
  template<int N, class T>
  constexpr char f(char (*)[([] { return G{}.template T<int>(); }())], T) { return 0; }
  template<int N> constexpr long f(char (*)[3], ...) { return 1; }

  char a[3]{};
  static_assert(sizeof(f<6>(&a, 1.0)) == sizeof(char));
  static_assert(f<6>(&a, 1.0) == 0);
}
#endif

#ifdef __clang__
namespace enable_if_overloads
{
  template<class T>
    __attribute__((enable_if(sizeof([] { return T{}; }()) == sizeof(char), "")))
  constexpr int pick(T) { return 1; }
  template<class T>
    __attribute__((enable_if(sizeof([](T x) { return x; }(T{})) == sizeof(int), "")))
  constexpr int pick(T) { return 2; }

  static_assert(pick('a') == 1);
  static_assert(pick(1) == 2);
  static_assert(pick('b') == 1);
  static_assert(pick(2) == 2);
}

namespace enable_if_access_context
{
  class C
  {
    static constexpr int priv = 1;
  public:
    template<class T>
      __attribute__((enable_if(sizeof([] { return priv + T{}; }()) > 0, "")))
    static constexpr int f(T) { return 1; }
  };

  static_assert(C::f(1) == 1);
  static_assert(C::f('a') == 1);
}
#endif
