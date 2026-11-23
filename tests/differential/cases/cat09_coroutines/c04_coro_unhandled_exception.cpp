#include <coroutine>
    #include <cstdio>
    #include <exception>
    struct CatchTask {
      struct promise_type {
        int caught = 0;
        CatchTask get_return_object() {
          return CatchTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept {
          try { std::rethrow_exception(std::current_exception()); }
          catch (int v) { caught = v; }
        }
      };
      std::coroutine_handle<promise_type> h;
      ~CatchTask() { if (h) h.destroy(); }
    };
    CatchTask boom(int v) { if (v > 0) throw v; co_return; }
    int main() {
      CatchTask ct = boom(77); ct.h.resume();
      std::printf("%d:%d\n", ct.h.done(), ct.h.promise().caught);
      return 0;
    }
