//type:fp
//options_all:--gn 160100 --c++26
//remark:P3068R6: __builtin_uncaught_exceptions with a std:: definition
//
// A call of __builtin_uncaught_exceptions (which is replaced by a call of
// std::uncaught_exceptions) is evaluated during constant evaluation even if
// the library defines std::uncaught_exceptions inline without constexpr (that
// definition is not used).

namespace std {
  inline int uncaught_exceptions() noexcept { return -1; }
}

static_assert(__builtin_uncaught_exceptions() == 0);

struct S {
  int *p;
  constexpr ~S() { *p = __builtin_uncaught_exceptions(); }
};
constexpr int during_unwinding() {
  int n = -1;
  try {
    S s{&n};
    throw 1;
  } catch (int) {
  }
  return n;
}
static_assert(during_unwinding() == 1);
