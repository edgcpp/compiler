//options_all:--c++17 --g++
//type:fn
#pragma GCC target("avx,avx2,fma")

template <class>
struct integral_constant { constexpr operator bool() { return true; } };

constexpr integral_constant<bool> __is_complete_or_unbounded() { return {}; }

static_assert(__is_complete_or_unbounded(), "");
