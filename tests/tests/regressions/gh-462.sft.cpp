//type:fp
//options:--c++20:--c++20 --g++ --gnu_version=150200:--c++20 --clang --clang_version=210000:--ms_c++20 --microsoft_version=1950
//remark:decltype of a template parameter in a requires-expression parameter

// The parameter declarations of a requires-expression appearing outside of a
// concept definition did not record the information needed to substitute
// decltype applied to a template parameter.  The substituted parameter type
// was therefore unusable, and any requirement using the parameter in an
// operation was considered unsatisfied.  In a member template of a class
// template, decltype applied to a class-type template parameter of the
// enclosing class template also lost its id-expression form when the class
// template was instantiated, so the parameter got the const type of the
// template parameter object.

template <typename T, typename U> constexpr bool is_same = false;
template <typename T> constexpr bool is_same<T, T> = true;

struct NoConst {
  int operator()() { return 0; }
};

// F is a const lvalue (a template parameter object), but decltype(F) is the
// declared type of the template parameter, which is not const.
template <auto F>
inline constexpr bool const_callable_v = requires { F(); };
template <auto F>
inline constexpr bool non_const_callable_v =
    requires(decltype(F) f) { f(); };
static_assert(!const_callable_v<NoConst{}>);
static_assert(non_const_callable_v<NoConst{}>);

// A template parameter declared with the class type.
template <NoConst F>
constexpr bool typed_v = requires(decltype(F) f) { f(); };
static_assert(typed_v<NoConst{}>);

// The parameter has exactly the declared type.
template <auto F>
constexpr bool exact_type_v =
    requires(decltype(F) f) { requires is_same<decltype(f), NoConst>; };
static_assert(exact_type_v<NoConst{}>);

// Declarators built on the decltype type.
template <auto F>
constexpr bool ref_v = requires(decltype(F) &f) { f(); };
static_assert(ref_v<NoConst{}>);
template <auto F>
constexpr bool ptr_v = requires(decltype(F) *f) { (*f)(); };
static_assert(ptr_v<NoConst{}>);

// The parenthesized form denotes the const template parameter object.
template <auto F>
constexpr bool paren_v = requires(decltype((F)) f) { f(); };
static_assert(!paren_v<NoConst{}>);

// A nontype template parameter of non-class type.
template <auto N>
constexpr bool int_v = requires(decltype(N) n) { n + 1; };
static_assert(int_v<42>);

// A requires-expression in a function template body.
template <auto F> constexpr bool in_function() {
  return requires(decltype(F) f) { f(); };
}
static_assert(in_function<NoConst{}>());

// A requires-expression in a member of a class template.
template <auto F> struct S {
  static constexpr bool value = requires(decltype(F) f) { f(); };
  template <int I> static constexpr bool member_template() {
    return requires(decltype(F) f, decltype(I) i) { f() + i; };
  }
  template <int I> static constexpr bool member_exact_type() {
    return requires(decltype(F) f) {
      requires is_same<decltype(f), NoConst>;
    };
  }
  template <int I> static constexpr bool member_paren() {
    return requires(decltype((F)) f) { f(); };
  }
  template <int I> static constexpr bool out_of_class();
};
template <auto F> template <int I>
constexpr bool S<F>::out_of_class() {
  return requires(decltype(F) f) { f(); };
}
static_assert(S<NoConst{}>::value);
static_assert(S<NoConst{}>::member_template<1>());
static_assert(S<NoConst{}>::member_exact_type<1>());
static_assert(!S<NoConst{}>::member_paren<1>());
static_assert(S<NoConst{}>::out_of_class<1>());

// This already worked in a concept definition.
template <auto F>
concept NonConstCallable = requires(decltype(F) f) { f(); };
static_assert(NonConstCallable<NoConst{}>);
