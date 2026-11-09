//type:fp
//options:--c++17 --g++:--c++17 --clang:--ms_c++17 --microsoft --microsoft_version 1936:--ms_c++17 --microsoft --microsoft_version 1936 --dep_name
//remark:Access checking of default template arguments substituted while
//remark:rescanning a partial specialization.

// Names in a default template argument are access-checked in the context of
// the template declaration, but friends of the template do not get access in
// its default template arguments.  When the default argument is substituted
// while rescanning a partial specialization, an access failure must still be
// a substitution failure (and friendship of the template must not be used).

template<typename...> using void_t = void;

// Variable template detection idiom.
template<typename, template<typename...> typename TT, typename... P>
constexpr bool ok = false;
template<template<typename...> typename TT, typename... P>
constexpr bool ok<void_t<TT<P...>>, TT, P...> = true;

// Class template detection idiom.
template<typename T, typename = void> struct D {
  static constexpr bool value = false;
};
template<typename T> struct D<T, void_t<typename T::template A<int>>> {
  static constexpr bool value = true;
};

struct S {
private:
  using type = int;
  template<typename T, typename U> friend struct A;
  template<typename T, typename U> friend struct B;
};
struct P {
  using type = int;
};
struct N {
private:
  using type = int;
};

// Private member, template is a friend: still a substitution failure.
template<typename T, typename U = typename T::type> struct A {};
static_assert(!ok<void, A, S>, "");
static_assert(!ok<void, A, N>, "");
static_assert(ok<void, A, P>, "");

// Default argument named in a class template partial specialization.
template<typename T, typename U = typename T::type> struct B {};
template<typename T, typename = void> struct E {
  static constexpr bool value = false;
};
template<typename T> struct E<T, void_t<B<T>>> {
  static constexpr bool value = true;
};
static_assert(!E<S>::value, "");
static_assert(E<P>::value, "");

// A member template of a class that has access is fine.
struct M {
private:
  using type = int;
public:
  template<typename T, typename U = typename M::type> struct A {};
};
static_assert(D<M>::value, "");
