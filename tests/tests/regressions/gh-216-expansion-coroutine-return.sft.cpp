//type: fn
//options: --c++26 --no_warnings
//remark:[GH #216] A coroutine encloses returns in expansion statements.

namespace std {
  template<typename P = void>
  struct coroutine_handle {
    void *address() const;
    static coroutine_handle from_address(void *);
  };

  struct suspend_never {
    bool await_ready() noexcept { return true; }
    template<typename P>
    void await_suspend(coroutine_handle<P>) noexcept {}
    void await_resume() noexcept {}
  };

  template<typename... Ts>
  struct coroutine_traits {
    struct promise_type {
      void get_return_object() {}
      suspend_never initial_suspend() { return {}; }
      suspend_never final_suspend() noexcept { return {}; }
      void return_void() {}
      void unhandled_exception() {}
    };
  };
}

namespace return_before_co_return_empty {
  void f() {
    template for (int x : {}) { return; }
    co_return;
  }
}

namespace return_before_co_return_nonempty {
  void f() {
    template for (int x : {0}) { return; }
    co_return;
  }
}

namespace co_return_before_return_empty {
  void f() {
    co_return;
    template for (int x : {}) { return; }
  }
}

namespace co_return_before_return_nonempty {
  void f() {
    co_return;
    template for (int x : {0}) { return; }
  }
}

namespace co_return_in_empty_before_return {
  void f() {
    template for (int x : {}) { co_return; }
    return;
  }
}

namespace co_return_in_nonempty_before_return {
  void f() {
    template for (int x : {0}) { co_return; }
    return;
  }
}

namespace return_before_co_return_in_empty {
  void f() {
    return;
    template for (int x : {}) { co_return; }
  }
}

namespace return_before_co_return_in_nonempty {
  void f() {
    return;
    template for (int x : {0}) { co_return; }
  }
}
