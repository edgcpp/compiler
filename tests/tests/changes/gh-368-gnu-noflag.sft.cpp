//type:fp
//options_all:--gn 160100 --c++26
//options:--clear_flag=constexpr_exceptions:--no_exceptions:--c++20:--clear_flag=constexpr_exceptions;fn
//remark:GCC exception_ptr builtins without exceptions in constant evaluation
//
// __builtin_current_exception and __builtin_uncaught_exceptions (whose calls
// are replaced by calls of the std:: functions) can be evaluated during
// constant evaluation even if exceptions cannot be thrown there.  A direct
// call of a std::rethrow_exception that is not declared constexpr is then not
// a constant expression (the last case is expected to fail).

namespace std {
  namespace __exception_ptr {
    class exception_ptr {
      void* _M_exception_object;
    public:
      constexpr exception_ptr() noexcept : _M_exception_object(0) {}
      constexpr bool _M_is_null() const noexcept {
        return _M_exception_object == 0;
      }
    };
  }
  using __exception_ptr::exception_ptr;
  exception_ptr current_exception() noexcept;
  void rethrow_exception(exception_ptr) __attribute__ ((__noreturn__));
  int uncaught_exceptions() noexcept __attribute__ ((__pure__));
}

static_assert(__builtin_uncaught_exceptions() == 0);

constexpr bool no_current_exception() {
  return __builtin_current_exception()._M_is_null();
}
static_assert(no_current_exception());

#if TEST_NUMBER == 4
constexpr int rethrow() {
  std::rethrow_exception(std::exception_ptr());
  return 0;
}
static_assert(rethrow() == 0);  // Error: Not a constant expression.
#endif
