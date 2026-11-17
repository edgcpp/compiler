//type:fp
//options:--c++20
//remark:[GH #546] Parentheses around co_await and co_yield in generated C++
//
// Without parentheses in the IL, the C++-generating back end dropped
// parentheses that are needed around a co_await or co_yield expression used
// as an operand, around a co_await operand with lower precedence, and around
// a comma operand of co_yield.

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
};
struct task { using promise_type = promise; };

task outer() {
  int i = (co_await awaiter<S>{}).m;
  (co_yield S{}) = 1;
}

bool c;
task await_operand() {
  co_await (c ? awaiter<S>{} : awaiter<S>{});
}

int first();
S second();
task yield_operand() {
  co_yield (first(), second());
}

template<typename T>
task dependent() {
  (co_yield T{}) = 1;
  co_return;
}
template task dependent<S>();
