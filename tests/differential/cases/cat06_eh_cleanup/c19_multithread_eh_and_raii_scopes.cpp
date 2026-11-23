#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>
struct Guard {
  int* acc;
  int v;
  ~Guard() { *acc = *acc * 10 + v; }
};
static int scope_dance() {
  int acc = 0;
  for (int i = 0; i < 3; ++i) {
    Guard g{&acc, i + 1};
    if (i == 0) continue;
    if (i == 1) break;
  }
  {
    Guard g3{&acc, 7};
    goto after;
  }
after:
  return acc;
}
int main() {
  std::atomic<int> total{0};
  std::vector<std::thread> ts;
  for (int t = 1; t <= 4; ++t) {
    ts.emplace_back([t, &total]() {
      try {
        try {
          throw std::runtime_error("boom");
        } catch (const std::exception&) {
          throw;
        }
      } catch (const std::runtime_error&) {
        total.fetch_add(t * 10, std::memory_order_relaxed);
      }
    });
  }
  for (auto& th : ts) th.join();
  std::printf("%d %d\n", scope_dance(), total.load());
  return 0;
}
