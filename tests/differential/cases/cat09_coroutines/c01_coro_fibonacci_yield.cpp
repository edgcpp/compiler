#include <coroutine>
    #include <cstdio>
    #include <exception>
    struct IntGen {
      struct promise_type {
        int cur = 0;
        IntGen get_return_object() {
          return IntGen{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(int v) noexcept { cur = v; return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
      };
      std::coroutine_handle<promise_type> h;
      explicit IntGen(std::coroutine_handle<promise_type> hh) : h(hh) {}
      ~IntGen() { if (h) h.destroy(); }
      bool next(int* out) {
        h.resume();
        if (h.done()) return false;
        *out = h.promise().cur;
        return true;
      }
    };

    IntGen fib(int count) {
      int a = 1, b = 1;
      for (int i = 0; i < count; ++i) {
        co_yield a;
        int nxt = a + b; a = b; b = nxt;
      }
    }
    int main() {
      IntGen g = fib(8); int v = 0, sum = 0;
      while (g.next(&v)) sum = sum * 3 + v;
      std::printf("%d\n", sum);
      return 0;
    }
