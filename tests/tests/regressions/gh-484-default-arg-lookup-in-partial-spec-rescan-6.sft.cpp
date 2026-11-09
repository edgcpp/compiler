//type:fp
//options:--c++20 --g++:--c++20 --clang:--ms_c++20 --microsoft --microsoft_version 1936:--ms_c++20 --microsoft --microsoft_version 1936 --dep_name
//remark:Default template arguments of a concept substituted while
//remark:rescanning a partial specialization declared earlier.

// The default template argument of a concept is substituted in the context
// of the concept, even when that happens while rescanning a partial
// specialization declared before the entities the default argument uses.

template<class...> using void_t = void;

// Variable template detection idiom.
template<class, template<class...> class TT, class... P>
constexpr bool ok = false;
template<template<class...> class TT, class... P>
constexpr bool ok<void_t<TT<P...>>, TT, P...> = true;

// Class template detection idiom.
template<class T, class = void> struct is_ok {
  static constexpr bool value = false;
};

namespace lib {
template<class T> T&& declval() noexcept;
template<class T> struct identity { using type = T; };
}

template<class T, class U = decltype(&lib::declval<T&>())>
concept Addressable = sizeof(U) > 0;

template<class T, class U = typename lib::identity<T>::type,
         class W = decltype(&lib::declval<U&>())>
concept Chained = sizeof(W) > 0;

template<class T> requires Addressable<T> struct A {};
template<class T> requires Chained<T> struct B {};

template<class T> struct is_ok<T, void_t<A<T>>> {
  static constexpr bool value = true;
};

template<class T, class = void> constexpr bool via_concept = false;
template<class T>
constexpr bool via_concept<T, void_t<A<T>, B<T>>> = true;

struct V {};
struct N { void operator&() = delete; };

static_assert(ok<void, A, V>, "");
static_assert(!ok<void, A, N>, "");
static_assert(ok<void, B, V>, "");
static_assert(!ok<void, B, N>, "");
static_assert(is_ok<V>::value, "");
static_assert(!is_ok<N>::value, "");
static_assert(via_concept<V>, "");
static_assert(!via_concept<N>, "");

int main() {}
