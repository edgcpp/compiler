//options_all:--microsoft_version 1914 --ms_c++17
template <typename T> T&& Declval() noexcept;

using FP1 = int (*)(int);
using FP2 = int (*)(int) noexcept;

static_assert(!noexcept(Declval<FP1>()(1729)));
static_assert( noexcept(Declval<FP2>()(1729)));

struct X { };
using PMF1 = int (X::*)(int);
using PMF2 = int (X::*)(int) noexcept;

static_assert(!noexcept((Declval<X>().*Declval<PMF1>())(1729)));
static_assert( noexcept((Declval<X>().*Declval<PMF2>())(1729)));

static_assert(!noexcept((Declval<X*>()->*Declval<PMF1>())(1729)));
static_assert( noexcept((Declval<X*>()->*Declval<PMF2>())(1729)));
