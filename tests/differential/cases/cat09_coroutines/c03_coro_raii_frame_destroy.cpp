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

    static int log_events = 0;
    struct FrameGuard { int id;
      FrameGuard(int i):id(i){ log_events = log_events*10 + id; }
      ~FrameGuard(){ log_events = log_events*10 + (id + 5); } };
    IntGen guarded_seq() {
      FrameGuard g1(1);
      co_yield 10;
      FrameGuard g2(2);
      co_yield 20;
    }
    int main() {
      { IntGen g = guarded_seq(); int x = 0; g.next(&x); }
      std::printf("%d\n", log_events);
      return 0;
    }
