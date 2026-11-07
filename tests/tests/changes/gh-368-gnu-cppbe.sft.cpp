//type:cppbe
//options_all:--gn 160100 --c++26
//remark:C++-generating back end: GCC exception_ptr builtins
//match_regex:return __builtin_current_exception\(\);
//match_regex:return __builtin_uncaught_exceptions\(\);

namespace std {
  // As in the GCC C++ standard library, exception_ptr is not trivially
  // copyable, so a call that returns it is not just a call node.
  class exception_ptr {
    void* p;
  public:
    exception_ptr() noexcept : p(nullptr) {}
    exception_ptr(const exception_ptr& other) noexcept : p(other.p) {}
    ~exception_ptr() noexcept {}
  };
  exception_ptr current_exception() noexcept;
  int uncaught_exceptions() noexcept;
}

std::exception_ptr f() {
  try {
    throw 1;
  } catch (...) {
    return __builtin_current_exception();
  }
}

int g() {
  return __builtin_uncaught_exceptions();
}
