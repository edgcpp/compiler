//type:fp
//options_all:--clang --c++17
//remark:[GH #319] The macOS clang bases define __GCC_CONSTRUCTIVE_SIZE and __GCC_DESTRUCTIVE_SIZE
// 10/15/26  [GH #319]
//
// Clang 19 and later predefine __GCC_CONSTRUCTIVE_SIZE and
// __GCC_DESTRUCTIVE_SIZE, and libc++ (<__new/interference_size.h>, reached by
// <new>, <string>, <vector>, <algorithm>, ...) uses them unconditionally from
// C++17 on to define std::hardware_constructive_interference_size and
// std::hardware_destructive_interference_size.
//
// The values depend on the target (Apple clang 21 gives 64 and 256 for arm64,
// 64 and 64 for x86_64), so the test checks that the macros are defined and
// usable the way libc++ uses them, not their values.

#if defined(__APPLE__) && defined(__clang__)

#ifndef __GCC_DESTRUCTIVE_SIZE
#error "__GCC_DESTRUCTIVE_SIZE is not defined"
#endif
#ifndef __GCC_CONSTRUCTIVE_SIZE
#error "__GCC_CONSTRUCTIVE_SIZE is not defined"
#endif

typedef decltype(sizeof(0)) size_t;

// As in libc++'s <__new/interference_size.h>.
inline constexpr size_t hardware_destructive_interference_size =
    __GCC_DESTRUCTIVE_SIZE;
inline constexpr size_t hardware_constructive_interference_size =
    __GCC_CONSTRUCTIVE_SIZE;

// Both are positive powers of two, and the destructive size (the one that
// avoids false sharing) is not smaller than the constructive one.
static_assert(hardware_constructive_interference_size > 0 &&
              (hardware_constructive_interference_size &
               (hardware_constructive_interference_size - 1)) == 0,
              "constructive size is a power of two");
static_assert(hardware_destructive_interference_size > 0 &&
              (hardware_destructive_interference_size &
               (hardware_destructive_interference_size - 1)) == 0,
              "destructive size is a power of two");
static_assert(hardware_destructive_interference_size >=
              hardware_constructive_interference_size,
              "destructive size is not smaller than constructive size");

#endif

int main() { return 0; }
