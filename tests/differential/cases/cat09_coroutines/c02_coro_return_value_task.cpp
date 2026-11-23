#include <coroutine>
    #include <cstdio>
    #include <exception>
    struct Task {
      struct promise_type {
        int result = 0;
        Task get_return_object() {
          return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(int v) noexcept { result = v; }
        void unhandled_exception() { std::terminate(); }
      };
      std::coroutine_handle<promise_type> h;
      ~Task() { if (h) h.destroy(); }
    };
    struct DoubleAwait {
      int x;
      bool await_ready() const noexcept { return false; }
      bool await_suspend(std::coroutine_handle<>) const noexcept { return false; }
      int await_resume() const noexcept { return x * 2; }
    };
    Task compute(int base) {
      int a = co_await DoubleAwait{base};
      int b = co_await DoubleAwait{a + 3};
      co_return a + b;
    }
    int main() {
      Task t = compute(10);
      std::printf("%d\n", t.h.promise().result);
      return 0;
    }
