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

      IntGen seq_5(int start, int n) {
        int cur = start;
        for (int i = 0; i < n; ++i) {
          co_yield cur;
          cur = (cur * 3 + 7) % 1009;
        }
      }
      int main() {
        IntGen g = seq_5(5, 7);
        int x = 0, total = 0;
        while (g.next(&x)) total += x;
        std::printf("%d\n", total);
        return 0;
      }
