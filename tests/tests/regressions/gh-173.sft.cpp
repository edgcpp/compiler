//type:fp
//options:--c++20:--c++20 --gn 150200:--c++20 --clang_version 230100:--ms_c++20 --microsoft_version 1952

namespace variable_template_requirement
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<template<typename> class C>
  using A = decltype(C(0));

  template<template<typename> class C>
  constexpr bool ok = requires { typename A<C>; };

  static_assert(ok<S>);
  static_assert(!ok<B>);
}

namespace static_member_requirement
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<template<typename> class C>
  using A = decltype(C(0));

  template<template<typename> class C>
  struct X {
    static constexpr bool ok = requires { typename A<C>; };
  };

  static_assert(X<S>::ok);
  static_assert(!X<B>::ok);
}

namespace mismatched_parameter_positions
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T, int); };

  template<typename T, template<typename> class C>
  using A = decltype(C(0));

  template<template<typename> class C, template<typename> class D>
  constexpr bool ok = requires { typename A<int, C>; };

  static_assert(ok<S, B>);
  static_assert(!ok<B, S>);
}

namespace chained_aliases
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<template<typename> class C>
  using A = decltype(C(0));
  template<template<typename> class C>
  using D = A<C>;

  template<template<typename> class C>
  constexpr bool ok = requires { typename D<C>; };

  static_assert(ok<S>);
  static_assert(!ok<B>);
}

namespace variadic_call_alias
{
  template<typename T>
  T declval();

  template<typename F, typename... T>
  using A = decltype(declval<F>()(declval<T>()...));

  struct S { void operator()(int); };
  struct B { void operator()(); };

  template<typename F>
  constexpr bool ok = requires { typename A<F, int>; };

  static_assert(ok<S>);
  static_assert(!ok<B>);
}

namespace mixed_class_and_alias_arguments
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };
  template<typename T, typename U>
  struct P {};

  template<template<typename> class C>
  using A = decltype(C(0));
  template<typename T, template<typename> class C>
  using D = P<T, A<C>>;

  template<template<typename> class C>
  constexpr bool ok = requires { typename D<int, C>; };

  static_assert(ok<S>);
  static_assert(!ok<B>);
}

namespace alias_parameter_beyond_enclosing_list
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<typename T, template<typename> class C>
  using A = decltype(C(0));

  template<template<typename> class C>
  constexpr bool ok = requires { typename A<int, C>; };

  static_assert(ok<S>);
  static_assert(!ok<B>);
}

namespace function_template_deduction
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<typename T, template<typename> class C>
  using A = decltype(C(0));

  template<template<typename> class C>
  void f(A<int, C>*);

  void g() { f<S>(nullptr); }

  template<template<typename> class C>
  constexpr bool ok = requires { f<C>(nullptr); };

  static_assert(ok<S>);
  static_assert(!ok<B>);
}

namespace mismatched_parameter_kinds
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<typename T, template<typename> class C>
  using A = decltype(C(0));

  template<template<typename> class C, typename T>
  constexpr bool ok = requires { typename A<T, C>; };

  static_assert(ok<S, int>);
  static_assert(!ok<B, int>);
}

namespace pointer_to_decltype
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<typename T, template<typename> class C>
  using A = decltype(C(0))*;

  template<template<typename> class C>
  constexpr bool ok = requires { typename A<int, C>; };

  static_assert(ok<S>);
  static_assert(!ok<B>);
}

namespace qualified_decltype
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<typename T, template<typename> class C>
  using A = const decltype(C(0));

  template<template<typename> class C>
  constexpr bool ok = requires { typename A<int, C>; };

  static_assert(ok<S>);
  static_assert(!ok<B>);
}

namespace ordinary_alias_parameter_substitution
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<typename T, template<typename> class C>
  using A = decltype(C(T()));

  template<template<typename> class C>
  constexpr bool ok = requires { typename A<long, C>; };

  static_assert(ok<S>);
  static_assert(!ok<B>);

  template<typename T, typename U>
  constexpr bool same = false;
  template<typename T>
  constexpr bool same<T, T> = true;

  static_assert(same<A<long, S>, S<long>>);
}

namespace nested_template_template_binding
{
  template<typename T>
  struct B { B(T*); };

  template<typename T, template<typename> class C>
  using A = decltype((C(T()), C(T())));

  template<typename T>
  constexpr bool p = requires { typename A<T, B>; };

  template<typename T>
  struct S { S(T) requires (!p<T>); };

  template<typename T, template<typename> class C>
  constexpr bool ok = requires { typename A<T, C>; };

  static_assert(ok<int, S>);
  static_assert(!ok<int, B>);
}

namespace nested_type_parameter_binding
{
  template<typename T>
  struct B { B(T, T); };

  template<typename T, template<typename> class C>
  using A = decltype((C(T()), C(T())));

  template<typename T>
  constexpr bool p = requires { typename A<T*, B>; };

  template<typename T>
  struct S { S(T) requires (!p<T>); };

  template<typename T, typename U>
  constexpr bool same = false;
  template<typename T>
  constexpr bool same<T, T> = true;

  template<typename T, template<typename> class C>
  constexpr bool exact = same<A<T, C>, C<T>>;

  static_assert(exact<int, S>);
  static_assert(exact<long, S>);
}

namespace nested_nontype_parameter_binding
{
  template<int>
  struct K {};
  template<typename T>
  struct B { B(T, T); };

  template<int N, template<typename> class C>
  using A = decltype((C(K<N>()), C(K<N>())));

  template<typename T>
  constexpr bool p = requires { typename A<sizeof(T), B>; };

  template<typename T>
  struct S { S(T) requires (!p<T>); };

  template<int N, template<typename> class C>
  constexpr bool ok = requires { typename A<N, C>; };

  template<typename T, typename U>
  constexpr bool same = false;
  template<typename T>
  constexpr bool same<T, T> = true;

  template<int N, template<typename> class C>
  constexpr bool exact = same<A<N, C>, C<K<N>>>;

  static_assert(ok<0, S>);
  static_assert(!ok<0, B>);
  static_assert(exact<0, S>);
  static_assert(exact<2, S>);
}

namespace enclosing_template_template_parameter
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<typename T, template<typename> class C>
  using A = decltype(C(T()));

  template<template<typename> class D>
  struct O {
    template<typename U>
    static constexpr bool ok = requires { typename A<U, D>; };
  };

  static_assert(O<S>::ok<int>);
  static_assert(!O<B>::ok<int>);
}

namespace member_alias_of_enclosing_parameter
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<typename T, template<typename> class C>
  using A = decltype(C(T()));

  template<typename T, typename U>
  constexpr bool same = false;
  template<typename T>
  constexpr bool same<T, T> = true;

  template<template<typename> class D>
  struct O {
    template<typename U>
    using M = A<U, D>;
    template<typename U>
    static constexpr bool ok = requires { typename M<U>; };
  };

  static_assert(same<O<S>::M<int>, S<int>>);
  static_assert(O<S>::ok<int>);
  static_assert(!O<B>::ok<int>);
}

namespace trailing_return_type
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<typename T, template<typename> class C>
  using A = decltype(C(T()));

  template<template<typename> class D, typename U>
  auto f(int) -> A<U, D>;
  template<template<typename> class D, typename U>
  void f(...);

  template<typename T, typename U>
  constexpr bool same = false;
  template<typename T>
  constexpr bool same<T, T> = true;

  static_assert(same<decltype(f<S, int>(0)), S<int>>);
  static_assert(same<decltype(f<B, int>(0)), void>);
}

namespace constrained_member
{
  template<typename T>
  struct K { K(T); };

  template<typename T, template<typename> class C>
  using A = decltype(C(T()));

  template<typename T>
  struct S { void f() requires (sizeof(A<T, K>) > 0); };

  void g() { S<int>().f(); }
}

namespace constrained_guide_alias
{
  template<typename T>
  struct K { K(T); };

  template<typename T, template<typename> class C>
  using A = decltype(C(T()));

  template<typename T>
  struct S { S(T) requires (sizeof(A<T, K>) > 0); };

  S s(0);
}

namespace constrained_guide_requirement
{
  template<typename T>
  struct K { K(T); };

  template<typename T, template<typename> class C>
  using A = decltype(C(T()));

  template<typename T>
  struct S { S(T) requires (sizeof(A<T, K>) > 0); };

  template<typename T, template<typename> class C>
  constexpr bool ok = requires { typename A<T, C>; };

  static_assert(ok<int, S>);
}

namespace pack_expansion
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };
  template<typename... T>
  struct P {};

  template<template<typename> class... C>
  constexpr bool ok = requires { typename P<decltype(C(0))...>; };

  static_assert(ok<S, S>);
  static_assert(!ok<S, B>);
}

namespace pack_expansion_alias
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };
  template<typename... T>
  struct P {};

  template<template<typename> class... C>
  using A = P<decltype(C(0))...>;

  template<template<typename> class... C>
  constexpr bool ok = requires { typename A<C...>; };

  static_assert(ok<S, S>);
  static_assert(!ok<S, B>);
}

namespace variadic_template_template_argument
{
  template<typename T, typename U = int>
  struct S { S(T); };

  template<template<typename> class C>
  using A = decltype(C(0));

  using X = A<S>;
}

namespace variadic_template_template_parameter
{
  template<typename T, typename U = int>
  struct S { S(T); };

  template<template<typename> class C>
  using A = decltype(C(0));

  template<template<typename...> class D>
  constexpr bool ok = requires { typename A<D>; };

  static_assert(ok<S>);
}

namespace default_arguments_of_argument
{
  template<typename T, typename U>
  constexpr bool same = false;
  template<typename T>
  constexpr bool same<T, T> = true;

  template<typename T, typename U = long>
  struct S { S(T); };
  template<typename T, typename U = T*>
  struct D { D(T); };
  template<typename T, typename... U>
  struct V { V(T); };

  template<template<typename> class C>
  using A = decltype(C(0));

  static_assert(same<A<S>, S<int, long>>);
  static_assert(same<A<D>, D<int, int*>>);
  static_assert(same<A<V>, V<int>>);

  template<template<typename> class C>
  auto f() { return C{1.0}; }

  static_assert(same<decltype(f<S>()), S<double, long>>);
}

namespace pack_fold
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<template<typename> class... C>
  constexpr bool ok = requires { (C(0), ...); };

  static_assert(ok<S, S>);
  static_assert(!ok<S, B>);
  static_assert(!ok<B, S>);
}

namespace pack_expansion_with_trailing_defaults
{
  template<typename T, typename U = int>
  struct S { S(T); };
  template<typename T, typename U = long>
  struct D { D(T); };
  template<typename T, typename U = int>
  struct B { B(T*); };
  template<typename... T>
  struct P {};

  template<template<typename> class... C>
  using A = P<decltype(C(0))...>;

  template<template<typename> class... C>
  constexpr bool ok = requires { typename A<C...>; };
  template<template<typename> class... C>
  constexpr bool direct = requires { typename P<decltype(C(0))...>; };
  template<template<typename> class... C>
  constexpr bool fold = requires { (C(0), ...); };

  static_assert(ok<>);
  static_assert(ok<S, D>);
  static_assert(!ok<S, B>);
  static_assert(!ok<B, S>);
  static_assert(direct<S, D>);
  static_assert(!direct<S, B>);
  static_assert(!direct<B, S>);
  static_assert(fold<S, D>);
  static_assert(!fold<S, B>);
  static_assert(!fold<B, S>);

  template<typename T, typename U>
  constexpr bool same = false;
  template<typename T>
  constexpr bool same<T, T> = true;

  static_assert(same<A<S, D>, P<S<int, int>, D<int, long>>>);
}

namespace dependent_member_trailing_defaults
{
  struct X { using type = int; };
  template<typename T, typename U = typename T::type>
  struct S { S(T); };
  template<typename T, typename U = typename T::type, typename V = U*>
  struct D { D(T); };

  template<typename T, template<typename> class C>
  using A = decltype(C(T()));

  template<typename T, template<typename> class C>
  constexpr bool ok = requires { typename A<T, C>; };
  template<typename T, template<typename> class C>
  constexpr bool direct = requires { C(T()); };

  static_assert(ok<X, S>);
  static_assert(!ok<int, S>);
  static_assert(ok<X, D>);
  static_assert(!ok<int, D>);
  static_assert(direct<X, S>);
  static_assert(!direct<int, S>);
  static_assert(direct<X, D>);
  static_assert(!direct<int, D>);

  template<typename T, typename U>
  constexpr bool same = false;
  template<typename T>
  constexpr bool same<T, T> = true;

  static_assert(same<A<X, S>, S<X, int>>);
  static_assert(same<A<X, D>, D<X, int, int*>>);
}

namespace member_alias_of_specialization
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<typename X>
  struct O {
    template<template<typename> class C>
    using M = decltype(C(X()));
  };

  template<typename X, template<typename> class C>
  constexpr bool ok = requires { typename O<X>::template M<C>; };

  static_assert(ok<int, S>);
  static_assert(!ok<int, B>);
}

namespace nested_failing_alias_argument
{
  template<typename, typename>
  struct P {};
  template<typename T, typename V>
  struct B { B(T); };

  template<typename T, template<typename> class C>
  using A = P<decltype((C(T()), C(T()))), C<T>>;

  template<typename T, template<typename...> class D>
  constexpr bool p = requires { typename A<T, D>; };

  template<typename T>
  struct S {
    S(T) requires (!p<T*, B>);
  };

  template<typename T, template<typename> class C>
  constexpr bool ok = requires { typename A<T, C>; };

  static_assert(ok<int, S>);
}

namespace mismatched_variadic_template_template_parameter
{
  template<typename, typename>
  struct P {};
  template<typename T, typename V>
  struct B { B(T); };
  template<typename T, typename V = int>
  struct S { S(T); };

  template<typename T, template<typename> class C>
  using A = P<decltype(C(T())), C<T>>;

  template<template<typename...> class D, typename U>
  constexpr bool ok = requires { typename A<U, D>; };

  static_assert(ok<S, int>);
  static_assert(!ok<B, int>);
}

namespace enclosing_class_parameter_in_member_alias
{
  template<typename T>
  struct S { S(T); };
  template<typename T>
  struct B { B(T*); };

  template<template<typename> class C>
  struct O {
    template<typename X>
    using M = decltype(C(X()));
  };

  // GCC doesn't like this one
  template<typename X>
  constexpr bool ok_s = requires { typename O<S>::template M<X>; };
  template<typename X>
  constexpr bool ok_b = requires { typename O<B>::template M<X>; };

  static_assert(ok_s<int>);
  static_assert(!ok_b<int>);
}
