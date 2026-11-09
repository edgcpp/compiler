//type:fp
//options:--c++17 --g++:--c++17 --clang:--ms_c++17 --microsoft --microsoft_version 1936:--ms_c++17 --microsoft --microsoft_version 1936 --dep_name:--c++17
//remark:Default template arguments of a class template substituted while
//remark:rescanning a partial specialization declared earlier.

// When the default argument of a class template was substituted during the
// rescan of a partial specialization of a different template, names in the
// default argument were looked up in the context of the partial
// specialization.  Entities declared after the partial specialization (but
// before the class template) were therefore not found, and the substitution
// was wrongly considered to fail.
//
// Microsoft permissive mode does not do dependent name processing by default,
// so the problem only shows up there with --dep_name; the plain Microsoft
// mode is kept to check that nothing changes in that configuration.

template<class...> using void_t = void;

// Variable template detection idiom.
template<class, template<class...> class Template, class... P>
constexpr bool substitution_succeeds_impl = false;

template<template<class...> class Template, class... P>
constexpr bool substitution_succeeds_impl<void_t<Template<P...>>, Template,
                                          P...> = true;

template<template<class...> class Template, class... P>
constexpr bool substitution_succeeds =
    substitution_succeeds_impl<void, Template, P...>;

// Class template detection idiom.
template<class, template<class...> class Template, class... P>
struct is_detected_impl { static constexpr bool value = false; };

template<template<class...> class Template, class... P>
struct is_detected_impl<void_t<Template<P...>>, Template, P...> {
  static constexpr bool value = true;
};

template<template<class...> class Template, class... P>
constexpr bool is_detected = is_detected_impl<void, Template, P...>::value;

// Everything below is declared after the partial specializations.
namespace lib {
template<class T> T&& declval() noexcept;
template<class T> struct identity { using type = T; };
int f(int);
}

int g(long);

template<class T, class = decltype(&lib::declval<T&>())>
struct address_of_works {};

template<class T, class = typename lib::identity<T>::type>
struct uses_member_type {};

template<class T, class = decltype(lib::f(lib::declval<T>()))>
struct calls_lib_f {};

template<class T, class = decltype(g(lib::declval<T>()))>
struct calls_global_g {};

template<class T, class U = typename lib::identity<T>::type,
         class = decltype(&lib::declval<U&>())>
struct chained_defaults {};

namespace inner {
template<class T> T&& declval2() noexcept;
template<class T, class = decltype(declval2<T>() + 1)>
struct adds_one {};
}

struct Value {};

static_assert(substitution_succeeds<address_of_works, Value>, "");
static_assert(substitution_succeeds<uses_member_type, Value>, "");
static_assert(substitution_succeeds<calls_lib_f, int>, "");
static_assert(substitution_succeeds<calls_global_g, int>, "");
static_assert(substitution_succeeds<chained_defaults, Value>, "");
static_assert(substitution_succeeds<inner::adds_one, int>, "");

static_assert(is_detected<address_of_works, Value>, "");
static_assert(is_detected<uses_member_type, Value>, "");
static_assert(is_detected<calls_lib_f, int>, "");
static_assert(is_detected<calls_global_g, int>, "");
static_assert(is_detected<chained_defaults, Value>, "");
static_assert(is_detected<inner::adds_one, int>, "");

// Substitution failures must still be detected.
static_assert(!substitution_succeeds<address_of_works, void>, "");
static_assert(!substitution_succeeds<calls_lib_f, Value>, "");
static_assert(!substitution_succeeds<calls_global_g, Value>, "");
static_assert(!substitution_succeeds<chained_defaults, void>, "");
static_assert(!substitution_succeeds<inner::adds_one, Value>, "");

static_assert(!is_detected<address_of_works, void>, "");
static_assert(!is_detected<calls_lib_f, Value>, "");
static_assert(!is_detected<calls_global_g, Value>, "");
static_assert(!is_detected<chained_defaults, void>, "");
static_assert(!is_detected<inner::adds_one, Value>, "");

address_of_works<Value> a;
chained_defaults<Value> c;

int main() {
  return 0;
}
