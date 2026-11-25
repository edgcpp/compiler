//type:fp
//options:--c++11:--c++11 --gn 160200:--c++11 --clang_version 230100:--ms_c++20 --microsoft_version 1951
//options_all:--il_display --no_il_lower
//filter:edg-il-filter
//script:edg-first-stage-il

namespace non_type_param
{
  template<int>
  struct A { };

  struct B { };

  template<int N> struct C : A<N>, B {
    C() : A<N>(), B() { }
  };

  template struct C<1>;
}

namespace type_param
{
  template<typename>
  struct A { };

  struct B { };

  template<typename T> struct C : A<T>, B {
    C() : A<T>(), B() { }
  };

  template struct C<int>;
}

namespace template_param
{
  template<typename>
  struct X { };

  template<template<typename> class>
  struct A { };

  struct B { };

  template<template<typename> class P> struct C : A<P>, B {
    C() : A<P>(), B() { }
  };

  template struct C<X>;
}

namespace non_type_param_functionally_equivalent
{
  template<int>
  struct A { };

  struct B { };

  template<int N> struct C : A<N>, B {
    C(char) : A<+N>(), B() { }
    C(int) : A<N+0>(), B() { }
  };

  template struct C<1>;
}

namespace using_decltype
{
  template<typename T, typename U>
  struct X {
    static T a;
    static U b;
  };

  template<typename>
  struct A { };

  struct B { };

  template<typename T> struct C : A<decltype(T::a)>, A<decltype(T::b)>, B {
    C(char) : A<decltype(T::a)>(), A<decltype(T::b)>(), B() { }
    C(int) : B(), A<decltype(T::a)>(), A<decltype(T::b)>() { }
  };

  template struct C<X<int[1], int[2]>>;
}

/* @filter
func-scope constructor-init
  next, compiler_generated, kind, base_class, orig_type,
  ctor_init_range/start.seq
func-scope scope
  next, kind, routine.ptr, constructor_inits
 */
