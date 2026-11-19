//type:fp
//options:--c++20:--c++20 --gnu_version 150200:--c++20 --clang_version 230100:--ms_c++20 --microsoft_version 1950
//remark:[GH #546] Parentheses around co_await and co_yield in generated C++
//
// In configurations with PARENS_IN_IL set to FALSE, the C++-generating back
// end dropped parentheses that are needed around a co_await or co_yield
// expression used as an operand, and around some operands of co_await and
// co_yield.  The static assertions fail if the generated code groups an
// expression differently.  The dependent cases go through the IL (and so
// need the fix) only in configurations with ALL_TEMPLATE_INFO_IN_IL set to
// TRUE.

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

template<typename T, typename U>
constexpr bool is_same_v = false;
template<typename T>
constexpr bool is_same_v<T, T> = true;

struct suspend {
  bool await_ready() noexcept;
  void await_suspend(std::coroutine_handle<>) noexcept;
  void await_resume() noexcept;
};

template<typename T>
struct awaiter {
  bool await_ready() noexcept;
  void await_suspend(std::coroutine_handle<>) noexcept;
  T await_resume() noexcept;
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
};
struct task { using promise_type = promise; };

namespace await_expression {
  /* The awaited temporary is not parenthesized for any other reason, so
     this checks the co_await expression as the object of ".". */
  task f() {
    auto i = (co_await awaiter<S>{}).m;
    static_assert(is_same_v<decltype(i), int>);
    co_return;
  }
}

namespace await_operand {
  /* The operand of co_await is a cast-expression, so a comma or an
     assignment operand needs parentheses. */
  awaiter<int> x, y;
  task comma() {
    auto r = co_await (void(), awaiter<int>{});
    static_assert(is_same_v<decltype(r), int>);
    co_return;
  }
  task assignment() {
    auto r = co_await (x = y);
    static_assert(is_same_v<decltype(r), int>);
    co_return;
  }
}

namespace yield_expression {
  /* A yield-expression is an assignment-expression, so it needs
     parentheses as the operand of an assignment, conditional, or comma
     operation. */
  task assignment() {
    (co_yield S{}) = 1;
    co_return;
  }
  task operand() {
    auto a = (co_yield 1) + 2L;
    auto b = co_yield 1 + 2L;
    auto c = co_yield (1 + 2L);
    auto d = (co_yield 1 + 2L);
    static_assert(is_same_v<decltype(a), long>);
    static_assert(is_same_v<decltype(b), int>);
    static_assert(is_same_v<decltype(c), int>);
    static_assert(is_same_v<decltype(d), int>);
    co_return;
  }
  task conditional() {
    auto a = (co_yield 1) ? 2L : 3L;
    auto b = co_yield 1 ? 2L : 3L;
    auto c = co_yield (1 ? 2L : 3L);
    auto d = (co_yield 1 ? 2L : 3L);
    static_assert(is_same_v<decltype(a), long>);
    static_assert(is_same_v<decltype(b), int>);
    static_assert(is_same_v<decltype(c), int>);
    static_assert(is_same_v<decltype(d), int>);
    co_return;
  }
  task comma() {
    int i = 0;
    auto a = ((co_yield 1), 2L);
    auto b = (++i, (co_yield 1));
    static_assert(is_same_v<decltype(a), long>);
    static_assert(is_same_v<decltype(b), int>);
    co_return;
  }
}

namespace yield_operand {
  /* The operand of co_yield is an assignment-expression, so only a comma
     expression needs parentheses.  void() cannot be passed to
     yield_value, so a lost parenthesis makes the generated code
     ill-formed. */
  task comma() {
    co_yield (void(), 1);
    co_return;
  }
}

namespace dependent {
  /* These mirror the non-dependent tests, with both literal and dependent
     T operands. */
  template<typename T>
  task assignment() {
    (co_yield S{}) = 1;
    (co_yield T{}) = 1;
    co_return;
  }
  template task assignment<S>();

  template<typename T>
  task operand() {
    auto a = (co_yield 1) + 2L;
    auto b = (co_yield T{}) + 2L;
    auto c = co_yield 1 + 2L;
    auto d = co_yield T{} + 2L;
    auto e = co_yield (T{} + 2L);
    auto f = (co_yield T{} + 2L);
    static_assert(is_same_v<decltype(a), long>);
    static_assert(is_same_v<decltype(b), long>);
    static_assert(is_same_v<decltype(c), int>);
    static_assert(is_same_v<decltype(d), int>);
    static_assert(is_same_v<decltype(e), int>);
    static_assert(is_same_v<decltype(f), int>);
    co_return;
  }
  template task operand<int>();

  template<typename T>
  task conditional() {
    auto a = (co_yield 1) ? 2L : 3L;
    auto b = (co_yield T{}) ? 2L : 3L;
    auto c = co_yield T{} ? 2L : 3L;
    auto d = co_yield (T{} ? 2L : 3L);
    auto e = (co_yield T{} ? 2L : 3L);
    static_assert(is_same_v<decltype(a), long>);
    static_assert(is_same_v<decltype(b), long>);
    static_assert(is_same_v<decltype(c), int>);
    static_assert(is_same_v<decltype(d), int>);
    static_assert(is_same_v<decltype(e), int>);
    co_return;
  }
  template task conditional<int>();

  template<typename T>
  task comma() {
    T i{};
    auto a = ((co_yield 1), 2L);
    auto b = ((co_yield T{}), 2L);
    auto c = (++i, (co_yield T{}));
    static_assert(is_same_v<decltype(a), long>);
    static_assert(is_same_v<decltype(b), long>);
    static_assert(is_same_v<decltype(c), int>);
    co_return;
  }
  template task comma<int>();

  template<typename T>
  task yield_operand() {
    co_yield (void(), 1);
    co_yield (void(), T{});
    co_return;
  }
  template task yield_operand<S>();

  template<typename T>
  task await_expression() {
    auto a = (co_await awaiter<S>{}).m;
    auto b = (co_await awaiter<T>{}).m;
    static_assert(is_same_v<decltype(a), int>);
    static_assert(is_same_v<decltype(b), int>);
    co_return;
  }
  template task await_expression<S>();

  template<typename T>
  task await_operand() {
    awaiter<T> x, y;
    auto a = co_await (void(), awaiter<T>{});
    auto b = co_await (x = y);
    auto c = co_await (void(), awaiter<int>{});
    static_assert(is_same_v<decltype(a), T>);
    static_assert(is_same_v<decltype(b), T>);
    static_assert(is_same_v<decltype(c), int>);
    co_return;
  }
  template task await_operand<int>();
}
