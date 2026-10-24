//remark:has_unique_object_representations with volatile-qualified types
//type:fp
//options_all:--c++11
//options::--g++ --gnu_version=90400:--g++ --gnu_version=100100:--clang --clang_version=190000:--microsoft

// Standard behavior: cv-qualification does not affect the result.  g++
// before version 10 treated volatile-qualified non-class types as not having
// unique object representations.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 10
static_assert(!__has_unique_object_representations(volatile int), "vi");
static_assert(!__has_unique_object_representations(const volatile int),
              "cvi");
#else
static_assert(__has_unique_object_representations(volatile int), "vi");
static_assert(__has_unique_object_representations(const volatile int),
              "cvi");
#endif
static_assert(__has_unique_object_representations(int), "i");
static_assert(__has_unique_object_representations(const int), "ci");
