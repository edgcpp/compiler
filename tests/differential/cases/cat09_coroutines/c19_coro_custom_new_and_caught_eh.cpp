#include <coroutine>
#include <cstdio>
#include <cstdlib>
static int g_custom_new = 0;
static int g_custom_del = 0;
struct Task {
  struct promise_type {
    int result = 0;
    int failed = 0;
    static void* operator new(std::size_t sz) {
      ++g_custom_new;
      return std::malloc(sz);
    }
    static void operator delete(void* p) noexcept {
      ++g_custom_del;
      std::free(p);
    }
    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_value(int v) noexcept { result = v; }
    void unhandled_exception() noexcept { failed = 99; }
  };
  std::coroutine_handle<promise_type> h;
  ~Task() { if (h) h.destroy(); }
};
static Task run_coro(bool throw_escaped) {
  int acc = 10;
  try {
    co_await std::suspend_always{};
    throw 5;
  } catch (int x) {
    acc += x;
  }
  if (throw_escaped) throw 77;
  co_return acc;
}
int main() {
  int r1 = 0, f2 = 0;
  {
    Task t1 = run_coro(false);
    t1.h.resume();
    t1.h.resume();
    r1 = t1.h.promise().result;
    Task t2 = run_coro(true);
    t2.h.resume();
    t2.h.resume();
    f2 = t2.h.promise().failed;
  }
  std::printf("%d %d %d\n", r1, f2, g_custom_new == g_custom_del ? 1 : 0);
  return 0;
}
