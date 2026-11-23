#include <atomic>
    #include <cstdio>
    #include <thread>
    #include <vector>
    int main() {
      std::atomic<int> counter{0};
      std::vector<std::thread> ts;
      for (int t = 0; t < 4; ++t) {
        ts.emplace_back([&counter]() {
          for (int i = 0; i < 500; ++i) counter.fetch_add(1, std::memory_order_relaxed);
        });
      }
      for (auto& th : ts) th.join();
      std::printf("%d\n", counter.load());
      return 0;
    }
