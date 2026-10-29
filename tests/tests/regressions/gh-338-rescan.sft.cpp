//type:fp
//options:--c++20 --no_c++11_sfinae_ignore_access:--c++20 --c++11_sfinae_ignore_access

namespace dependent_declarations
{
  // Substituting T before deducing U must leave these closures nonreal.
  // No expression in their bodies refers to U: only declarations do.
  template<class T, class U>
  constexpr bool variable(T, U,
    char (*)[([] { [[maybe_unused]] U x; return 1; }())]) { return true; }

  template<class T, class U>
  constexpr bool array(T, U,
    char (*)[([] { [[maybe_unused]] U x[2]; return 1; }())]) { return true; }

  template<class T, class U>
  constexpr bool pointer(T, U,
    char (*)[([] { [[maybe_unused]] U *p; return 1; }())]) { return true; }

  template<class T, class U>
  constexpr bool alias(T, U,
    char (*)[([] { using A = U; return 1; }())]) { return true; }

  template<class T, class U>
  constexpr bool routine(T, U,
    char (*)[([] { void local(U); return 1; }())]) { return true; }

  // These types are nondependent; only their initializers refer to U.
  template<class T, class U>
  constexpr bool initializer(T, U,
    char (*)[([] { [[maybe_unused]] constexpr int x = sizeof(U);
                   return 1; }())]) { return true; }

  template<class T, class U>
  constexpr bool dynamic_initializer(T, U,
    char (*)[([] { [[maybe_unused]] int x = sizeof(U);
                   return 1; }())]) { return true; }

  constexpr bool test()
  {
    char a[1]{};
    variable<int>(1, 2L, &a);
    variable<int>(1, 'x', &a);
    array<int>(1, 2L, &a);
    array<int>(1, 'x', &a);
    pointer<int>(1, 2L, &a);
    pointer<int>(1, 'x', &a);
    alias<int>(1, 2L, &a);
    alias<int>(1, 'x', &a);
    routine<int>(1, 2L, &a);
    routine<int>(1, 'x', &a);
    initializer<int>(1, 2L, &a);
    initializer<int>(1, 'x', &a);
    dynamic_initializer<int>(1, 2L, &a);
    dynamic_initializer<int>(1, 'x', &a);
    return true;
  }
  static_assert(test());
}

namespace concept_template_binding
{
  // A lambda rescan must restore the concept's arguments, so instantiating
  // its generic call operator still sees the bound template-template argument.
  template<class T> T declare();

  template<class It, template<class> class Template>
  concept D = requires {
    []<class T1>(Template<T1>&) {}(declare<It&>());
  };

  template<class> struct B {};
  template<class> struct Other {};
  template<class> struct Adapter;
  template<D<B> T> struct Adapter<T> {};

  template struct Adapter<B<int>>;
  template struct Adapter<B<char>>;
  static_assert(D<B<int>, B>);
  static_assert(D<Other<long>, Other>);
  static_assert(!D<int, B>);
}

namespace nested_pack_constraint
{
  // The first lambda rescan must restore the concrete arguments on the
  // instantiation scope, even when an access-checking scope is above it.
  // Instantiating the generic lambda then needs those enclosing arguments.
  template<class... Ts>
    requires ([] { return (sizeof(Ts) + ... + 0); }() > 0)
          && requires {
               []<class... Us>() { return sizeof...(Ts); }
                 .template operator()<char>();
             }
  struct C { static constexpr int value = sizeof...(Ts); };

  static_assert(C<int, char>::value == 2);
  static_assert(C<long>::value == 1);
  static_assert(C<int, char>::value == 2);
}
