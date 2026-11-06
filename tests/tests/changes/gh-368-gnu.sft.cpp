//type:fp
//options_all:--gn 160100 --c++26
//remark:P3068R6: std::exception_ptr in constant evaluation (GCC library model)

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

#if !__has_builtin(__builtin_eh_ptr_adjust_ref) || \
    !__has_builtin(__builtin_current_exception) || \
    !__has_builtin(__builtin_uncaught_exceptions)
#error The exception_ptr builtins should be available.
#endif

constexpr int h1() {
  std::exception_ptr p;
  try { throw 42; } catch (...) { p = __builtin_current_exception(); }
  if (!p) return 1;
  try { std::rethrow_exception(p); } catch (int i) { return i; }
  return 2;
}
static_assert(h1() == 42);

constexpr int h2() {
  auto p = std::make_exception_ptr(7);
  std::exception_ptr q = p;
  try { std::rethrow_exception(q); } catch (int i) { return i + (p == q); }
  return 0;
}
static_assert(h2() == 8);

constexpr bool h3() { return !__builtin_current_exception(); }
static_assert(h3());

constexpr int h4() {
  try {
    throw 1;
  } catch (...) {
    try {
      throw 2;
    } catch (...) {
      try {
        std::rethrow_exception(__builtin_current_exception());
      } catch (int i) {
        if (i != 2) return 0;
      }
    }
    try {
      std::rethrow_exception(__builtin_current_exception());
    } catch (int i) {
      return i;
    }
  }
  return 0;
}
static_assert(h4() == 1);

struct Cnt {
  int* n;
  constexpr Cnt(int* n) : n(n) {}
  constexpr Cnt(const Cnt& o) : n(o.n) {}
  constexpr ~Cnt() { ++*n; }
};
constexpr int h5() {
  int n = 0;
  {
    std::exception_ptr p;
    try { throw Cnt(&n); } catch (...) { p = __builtin_current_exception(); }
    if (n != 0) return -1;
  }
  return n;
}
static_assert(h5() == 1);

constexpr int h6() {
  std::exception_ptr p = std::make_exception_ptr(5);
  std::exception_ptr q = static_cast<std::exception_ptr&&>(p);
  return (!p && q) ? 1 : 0;
}
static_assert(h6() == 1);

constexpr int h7() {
  try {
    std::rethrow_exception(std::make_exception_ptr(9));
  } catch (int i) {
    return i;
  }
  return 0;
}
static_assert(h7() == 9);

constexpr int h8() {
  int n = 0;
  try {
    std::rethrow_exception(std::make_exception_ptr(Cnt(&n)));
  } catch (const Cnt&) {
  }
  return n;
}
static_assert(h8() == 2);

struct Uncaught {
  int* n;
  constexpr ~Uncaught() {
    *n = __builtin_uncaught_exceptions();
  }
};
constexpr int h9() {
  int n = -1;
  try {
    Uncaught u{&n};
    throw 1;
  } catch (int) {
    if (__builtin_uncaught_exceptions() != 0) return -2;
  }
  return n;
}
static_assert(h9() == 1);
static_assert(__builtin_uncaught_exceptions() == 0);

constexpr bool thrower() { throw 1; }
constexpr int constant_p_throws() {
  try {
    return __builtin_constant_p(thrower()) + __builtin_uncaught_exceptions();
  } catch (int) {
    return -1;
  }
}
static_assert(constant_p_throws() == 0);

constexpr int assume_throws() {
  try {
    throw 5;
  } catch (int i) {
    [[assume((throw, true))]];
    return i + __builtin_uncaught_exceptions();
  }
}
static_assert(assume_throws() == 5);

// While a handler parameter is initialized, the exception is still uncaught
// and the handler is not yet active.
struct Inspect {
  int uncaught = -1;
  bool current = true;
  constexpr Inspect() {}
  constexpr Inspect(const Inspect&)
  : uncaught(__builtin_uncaught_exceptions()),
    current(!!__builtin_current_exception()) {}
};
constexpr int handler_init() {
  try {
    throw Inspect();
  } catch (Inspect i) {
    return i.uncaught * 100 + i.current * 10 + __builtin_uncaught_exceptions();
  }
}
static_assert(handler_init() == 100);

// An evaluation whose failure is ignored cannot release an exception that
// existed before it (e.g., by destroying a std::exception_ptr that it did not
// create); that makes the evaluation fail instead.
int nonconstexpr_int;
constexpr int ignored_release() {
  auto p = std::make_exception_ptr(7);
  void *obj = nullptr;
  try { std::rethrow_exception(p); } catch (int &i) { obj = &i; }
  int r = __builtin_constant_p((p.~exception_ptr(), nonconstexpr_int));
  r += __builtin_constant_p((__builtin_eh_ptr_adjust_ref(obj, -1), 1));
  [[assume((__builtin_eh_ptr_adjust_ref(obj, -1), true))]];
  try { std::rethrow_exception(p); } catch (int i) { return r + i; }
  return -1;
}
static_assert(ignored_release() == 7);
