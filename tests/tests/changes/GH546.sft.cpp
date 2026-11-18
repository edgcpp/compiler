//type:fp
//options:--c++20
//remark:[GH #546] Parentheses around co_await and co_yield in generated C++
//
// With PARENS_IN_IL=0, the C++-generating back end dropped parentheses that
// are needed around a co_await or co_yield expression used as an operand,
// around a co_await operand with lower precedence, and around a comma
// operand of co_yield.  The static assertions fail if the generated code
// groups an expression differently.  The dependent cases go through the IL
// (and so need the fix) only with ALL_TEMPLATE_INFO_IN_IL=1.

namespace std {
  template<typename = void> struct coroutine_handle;
  template<> struct coroutine_handle<> { void *address(); };
  template<typename> struct coroutine_handle {
    static coroutine_handle from_address(void *);
    operator coroutine_handle<>() noexcept;
  };
  template<typename R, typename...> struct coroutine_traits {
    using promise_type = typename R::promise_type;
  };
  template<class T, class U> struct is_same { static constexpr bool value = false; };
  template<class T> struct is_same<T, T> { static constexpr bool value = true; };
}

struct suspend {
  bool await_ready() noexcept;
  void await_suspend(std::coroutine_handle<>) noexcept;
  void await_resume() noexcept;
};

template<typename R>
struct awaiter {
  bool await_ready() noexcept;
  void await_suspend(std::coroutine_handle<>) noexcept;
  R await_resume() noexcept;
};

struct S {
  int m;
  void operator=(int);
};

struct task;
struct promise {
  suspend initial_suspend() noexcept;
  suspend final_suspend() noexcept;
  void return_void();
  void unhandled_exception();
  task get_return_object();
  awaiter<S> yield_value(S);
  awaiter<int> yield_value(int);
  awaiter<short> yield_value(long);  // A different type, to catch "co_yield (x + 2L)".
};
struct task { using promise_type = promise; };

task outer() {
  auto i = (co_await awaiter<S>{}).m;
  static_assert(std::is_same<decltype(i), int>::value);
  (co_yield S{}) = 1;
  auto v = (co_yield 1) + 2L;
  static_assert(std::is_same<decltype(v), long>::value);
}

bool c;
task await_operand() {
  auto r = co_await (c ? awaiter<int>{} : awaiter<int>{});
  static_assert(std::is_same<decltype(r), int>::value);
}

long first();
int second();
task yield_operand() {
  auto v = co_yield (first(), second());
  static_assert(std::is_same<decltype(v), int>::value);
}

template<typename T>
task dependent() {
  (co_yield T{}) = 1;
  auto a = (co_yield 1) + 2L;
  static_assert(std::is_same<decltype(a), long>::value);
  auto b = (co_yield T{}.m) + 2L;
  static_assert(std::is_same<decltype(b), long>::value);
  co_return;
}
template task dependent<S>();
