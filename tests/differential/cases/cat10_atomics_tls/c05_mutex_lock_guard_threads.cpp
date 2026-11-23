#include <cstdio>
    #include <mutex>
    #include <thread>
    #include <vector>
    int main() {
      std::mutex mu; int shared_total = 0;
      std::vector<std::thread> pool;
      for (int w = 1; w <= 4; ++w) {
        pool.emplace_back([&, w]() {
          for (int k = 0; k < 100; ++k) {
            std::lock_guard<std::mutex> lk(mu);
            shared_total += w;
          }
        });
      }
      for (auto& t : pool) t.join();
      std::printf("%d\n", shared_total);
      return 0;
    }
