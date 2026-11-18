namespace std {
  template <typename _Ret, typename... _Ts>
  struct coroutine_traits
  {
    using promise_type = typename _Ret::promise_type;
  };

  template <class = void>
  struct coroutine_handle {
    static constexpr coroutine_handle from_address(void* const) noexcept { return {}; }
    constexpr operator coroutine_handle<>() const noexcept { return {}; }
  };

  struct suspend_never {
    constexpr bool await_ready() const noexcept { return true; }
    constexpr void await_suspend(coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
  };

  struct suspend_always {
    constexpr bool await_ready() const noexcept { return false; }
    constexpr void await_suspend(coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
  };

  template <class T>
  struct generator {
    struct promise_type {
      generator get_return_object() noexcept { return generator{}; }
      suspend_always initial_suspend() noexcept { return {}; }
      suspend_always final_suspend() noexcept { return {}; }
      void unhandled_exception() noexcept {}
      suspend_always yield_value(const T&) noexcept { return {}; }
      void return_void() noexcept {}
    };
  };
}
