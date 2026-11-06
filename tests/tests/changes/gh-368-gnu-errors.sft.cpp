//type:fn
//options_all:--gn 160100 --c++26
//remark:P3068R6: Invalid std::exception_ptr uses in constant evaluation

namespace std {
  class exception {
  public:
    constexpr exception() noexcept {}
    constexpr virtual ~exception() noexcept {}
    constexpr virtual const char* what() const noexcept { return "exc"; }
  };
  namespace __exception_ptr { class exception_ptr; }
  using __exception_ptr::exception_ptr;
  exception_ptr current_exception() noexcept;
  template<typename _Ex> constexpr exception_ptr make_exception_ptr(_Ex)
    noexcept;
  void rethrow_exception(exception_ptr) __attribute__ ((__noreturn__));
  int uncaught_exceptions() noexcept __attribute__ ((__pure__));
  namespace __exception_ptr {
    using std::rethrow_exception;
    class exception_ptr {
      void* _M_exception_object;
      [[__gnu__::__gnu_inline__]]
      constexpr inline explicit exception_ptr(void* __e) noexcept
      : _M_exception_object(__e) {
        if (_M_exception_object) {
#if __has_builtin(__builtin_eh_ptr_adjust_ref)
          if consteval {
            __builtin_eh_ptr_adjust_ref(_M_exception_object, 1);
            return;
          }
#endif
          _M_addref();
        }
      }
      void _M_addref() noexcept;
      void _M_release() noexcept;
      friend exception_ptr std::current_exception() noexcept;
      friend void std::rethrow_exception(exception_ptr);
      template<typename _Ex>
      friend constexpr exception_ptr std::make_exception_ptr(_Ex) noexcept;
    public:
      constexpr exception_ptr() noexcept : _M_exception_object(0) {}
      constexpr exception_ptr(const exception_ptr& __other) noexcept
      : _M_exception_object(__other._M_exception_object) {
        if (_M_exception_object) {
#if __has_builtin(__builtin_eh_ptr_adjust_ref)
          if consteval {
            __builtin_eh_ptr_adjust_ref(_M_exception_object, 1);
            return;
          }
#endif
          _M_addref();
        }
      }
      constexpr exception_ptr(decltype(nullptr)) noexcept
      : _M_exception_object(nullptr) {}
      constexpr exception_ptr(exception_ptr&& __o) noexcept
      : _M_exception_object(__o._M_exception_object) {
        __o._M_exception_object = nullptr;
      }
      constexpr exception_ptr& operator=(const exception_ptr& __other)
        noexcept {
        exception_ptr(__other).swap(*this);
        return *this;
      }
      constexpr exception_ptr& operator=(exception_ptr&& __other) noexcept {
        exception_ptr(static_cast<exception_ptr&&>(__other)).swap(*this);
        return *this;
      }
      constexpr ~exception_ptr() noexcept {
        if (_M_exception_object) {
#if __has_builtin(__builtin_eh_ptr_adjust_ref)
          if consteval {
            __builtin_eh_ptr_adjust_ref(_M_exception_object, -1);
            return;
          }
#endif
          _M_release();
        }
      }
      constexpr void swap(exception_ptr& __other) noexcept {
        void* __tmp = _M_exception_object;
        _M_exception_object = __other._M_exception_object;
        __other._M_exception_object = __tmp;
      }
      constexpr explicit operator bool() const noexcept {
        return _M_exception_object;
      }
      friend constexpr bool operator==(const exception_ptr&,
                                       const exception_ptr&) noexcept
        = default;
    };
  }
  template<typename _Ex>
  constexpr exception_ptr make_exception_ptr(_Ex __ex) noexcept {
    try {
      throw __ex;
    } catch(...) {
#if __has_builtin(__builtin_current_exception)
      return __builtin_current_exception();
#else
      return current_exception();
#endif
    }
  }
}

constexpr int k1() {
  std::rethrow_exception(std::exception_ptr());
  return 0;
}
static_assert(k1() == 0);  // error: null exception_ptr

[[maybe_unused]] constexpr std::exception_ptr ep =
    std::make_exception_ptr(3);  // error

constexpr int k3() {
  std::rethrow_exception(std::make_exception_ptr(9));
  return 0;
}
static_assert(k3() == 0);  // error: uncaught int

constexpr bool leaked() {
  new std::exception_ptr(std::make_exception_ptr(4));
  return true;
}
static_assert(leaked());  // error: exception object not destroyed

struct ThrowingDtor {
  constexpr ~ThrowingDtor() noexcept(false) { throw 1; }
};
constexpr int exception_dtor_throws() {
  try { throw ThrowingDtor(); } catch (...) { throw 2; }
  return 0;
}
static_assert(exception_dtor_throws() == 0);  // error: std::terminate

// std::current_exception and std::uncaught_exceptions are not constexpr
// functions (P3842R2), unlike the builtins that implement them.
constexpr bool current() {
  try { throw 1; } catch (...) { return !!std::current_exception(); }
  return false;
}
static_assert(current());  // error: call to non-constexpr function
constexpr int uncaught() { return std::uncaught_exceptions(); }
static_assert(uncaught() == 0);  // error: call to non-constexpr function
