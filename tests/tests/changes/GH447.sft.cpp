//type:fp
//options:--c++23
//remark:[GH #447] co_await has the value category of await_resume()
//
// A co_await or co_yield expression was always treated as a prvalue, so
// assigning to the result of an await_resume() that returns an lvalue
// reference was rejected.  Also covers awaiters whose await_ready and
// await_resume calls are folded.

namespace std {
  template <typename = void> struct coroutine_handle;
  template <> struct coroutine_handle<> { void *address(); };
  template <typename> struct coroutine_handle {
    static coroutine_handle from_address(void *);
    operator coroutine_handle<>() noexcept;
  };
  template <typename R, typename...> struct coroutine_traits {
    using promise_type = typename R::promise_type;
  };
  template <class T, class U> struct is_same { static constexpr bool value = false; };
  template <class T> struct is_same<T, T> { static constexpr bool value = true; };
} // namespace std

struct suspend {
  bool await_ready() noexcept;
  void await_suspend(std::coroutine_handle<>) noexcept;
  void await_resume() noexcept;
};

template <class R> struct awaiter {
  bool await_ready() noexcept;
  void await_suspend(std::coroutine_handle<>) noexcept;
  R await_resume() noexcept;
};

struct task;

struct promise {
  suspend initial_suspend() noexcept;
  suspend final_suspend() noexcept;
  void return_void();
  void unhandled_exception();
  task get_return_object();
  awaiter<int &> yield_value(int);
};

struct task {
  using promise_type = promise;
};

task f() {
  (co_await awaiter<bool &>{}) = false;
  bool *p = &co_await awaiter<bool &>{};
  decltype(auto) a = co_await awaiter<bool &>{};
  decltype(auto) b = co_await awaiter<bool &&>{};
  decltype(auto) c = co_await awaiter<bool>{};
  static_assert(std::is_same<decltype(a), bool &>::value);
  static_assert(std::is_same<decltype(b), bool &&>::value);
  static_assert(std::is_same<decltype(c), bool>::value);
  int &r = co_yield 1;
  decltype(auto) d = co_yield 1;
  static_assert(std::is_same<decltype(d), int &>::value);
}

// When await_ready and await_resume are both constexpr and await_resume
// returns a reference, both calls are folded.  The awaiter must then not be
// cloned for await_suspend (lowering used to fail with an internal error).
bool flag;
template <bool Ready> struct constexpr_ref_awaiter {
  constexpr bool await_ready() noexcept { return Ready; }
  void await_suspend(std::coroutine_handle<>) noexcept;
  constexpr bool &await_resume() noexcept { return flag; }
};

task g() {
  (co_await constexpr_ref_awaiter<true>{}) = false;
  (co_await constexpr_ref_awaiter<false>{}) = false;
  bool x = co_await constexpr_ref_awaiter<true>{};
  (void)x;
}
