//type:fp
//options:--c++17 --g++:--c++17 --clang:--ms_c++17 --microsoft --microsoft_version 1936:--ms_c++17 --microsoft --microsoft_version 1936 --dep_name:--c++17
//remark:Default template arguments of a class template substituted while
//remark:rescanning partial specializations in other contexts.

// Companion to gh-484-default-arg-lookup-in-partial-spec-rescan.sft.cpp: the
// partial specialization is a member template, is reached through an alias
// template, or names a class template whose default argument refers to
// static members and enumerators declared after the partial specialization.
//
// Microsoft permissive mode does not do dependent name processing by default,
// so the problem only shows up there with --dep_name; the plain Microsoft
// mode is kept to check that nothing changes in that configuration.

template<class...> using void_t = void;

struct traits {
  template<class, template<class...> class Template, class... P>
  struct detect { static constexpr bool value = false; };

  template<template<class...> class Template, class... P>
  struct detect<void_t<Template<P...>>, Template, P...> {
    static constexpr bool value = true;
  };
};

template<template<class...> class Template, class... P>
using detect_t = traits::detect<void, Template, P...>;

template<class, template<class...> class Template, class... P>
constexpr int pick = 0;

template<template<class...> class Template, class... P>
constexpr int pick<void_t<Template<P...>>, Template, P...> = 1;

// Declared after the partial specializations.
namespace lib {
template<class T> T&& declval() noexcept;
struct holder { static constexpr int size = 4; };
enum class kind { a, b };
template<int N> struct int_c { static constexpr int value = N; };
}

template<class T, class = lib::int_c<lib::holder::size>,
         class = decltype(lib::declval<T>().member)>
struct has_member {};

template<class T, class = lib::int_c<(int)lib::kind::b>,
         class = decltype(sizeof(T))>
struct complete_with_kind {};

struct WithMember { int member; };
struct WithoutMember {};
struct Incomplete;

static_assert(traits::detect<void, has_member, WithMember>::value, "");
static_assert(!traits::detect<void, has_member, WithoutMember>::value, "");
static_assert(detect_t<has_member, WithMember>::value, "");
static_assert(!detect_t<has_member, WithoutMember>::value, "");
static_assert(pick<void, has_member, WithMember> == 1, "");
static_assert(pick<void, has_member, WithoutMember> == 0, "");

static_assert(detect_t<complete_with_kind, WithMember>::value, "");
static_assert(pick<void, complete_with_kind, WithoutMember> == 1, "");
static_assert(!detect_t<complete_with_kind, Incomplete>::value, "");

int main() {
  return 0;
}
