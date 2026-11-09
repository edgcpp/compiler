//type:fp
//options:--c++17 --g++:--c++17 --clang:--ms_c++17 --microsoft --microsoft_version 1936:--ms_c++17 --microsoft --microsoft_version 1936 --dep_name
//remark:Default template arguments of a variable template substituted while
//remark:rescanning a partial specialization declared earlier.

// Like default-arg-lookup-in-partial-spec-rescan.sft.cpp, but the template
// whose default argument refers to later-declared entities is a variable
// template (a member of a class template, or at namespace scope).

template<class...> using void_t = void;

template<class, template<class...> class TT, class... P>
constexpr bool ok = false;
template<template<class...> class TT, class... P>
constexpr bool ok<void_t<decltype(TT<P...>::template value<P...>)>, TT, P...> =
    true;


namespace lib {
template<class T> T&& declval() noexcept;
template<class T> struct identity { using type = T; };
int f(int);
}

// Member variable template of a class template.
template<class T> struct W {
  template<class U, class = decltype(&lib::declval<U&>())>
  static constexpr bool value = sizeof(U) > 0;
};

// Chained defaults and a call to a later-declared function.
template<class T> struct W2 {
  template<class U, class X = typename lib::identity<U>::type,
           class = decltype(lib::f(X()))>
  static constexpr int value = 1;
};

struct V {};
struct NoAddr { void operator&() = delete; };

static_assert(ok<void, W, V>, "");
static_assert(!ok<void, W, NoAddr>, "");
static_assert(ok<void, W2, int>, "");
static_assert(!ok<void, W2, V>, "");
