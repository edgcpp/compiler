//type:fp
//options:--c++20:--c++20 --g++ --gnu_version=150200:--c++20 --clang --clang_version=210000:--ms_c++20 --microsoft_version=1950
//remark:class-type template parameter of an enclosing template in a signature

// When an enclosing template was instantiated, references to a class-type
// template parameter of that template in the signature of a member template
// were replaced by the template parameter object.  Substituting the member
// template's own template arguments then turned the object into a prvalue:
// Taking its address made deduction fail, and other uses resulted in a
// function type inconsistent with that of the template.

struct A { int i = 0; };
constexpr A a1{1};

template <typename T, typename U> constexpr bool is_same = false;
template <typename T> constexpr bool is_same<T, T> = true;

// A member template of a class template.
template <A V> struct S {
  static constexpr const A *p = &V;
  template <typename T> static constexpr auto f(T t) -> decltype(t, &V) {
    return &V;
  }
  template <typename T> static auto g(T t) -> decltype(t, V);
  template <typename T> static auto h(T t) -> decltype(t, V.i);
};
static_assert(is_same<decltype(S<a1>::f(0)), const A *>);
static_assert(S<a1>::f(0) == S<a1>::p);
static_assert(is_same<decltype(S<a1>::g(0)), const A &>);
static_assert(is_same<decltype(S<a1>::h(0)), const int &>);

// A generic lambda in a function template.
template <A V> constexpr auto mk() {
  return [](auto t) -> decltype(t, &V) { return &V; };
}
static_assert(mk<a1>()(0) == mk<a1>()(0L));

// A member template of a class template partial specialization.
template <typename T, typename U> struct X;
template <A V> struct Tag { };
template <A V, typename U> struct X<Tag<V>, U> {
  template <typename T> static auto f(T t) -> decltype(t, U(), &V);
};
static_assert(is_same<decltype(X<Tag<a1>, int>::f(0)), const A *>);

// The type of the template parameter is deduced.
struct NoConst {
  int operator()() { return 0; }
};
template <auto F> struct D {
  template <typename T> static auto g(T t) -> decltype(t, F);
};
static_assert(is_same<decltype(D<NoConst{}>::g(0)), const NoConst &>);
