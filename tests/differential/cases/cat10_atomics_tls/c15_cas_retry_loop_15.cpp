#include <atomic>
      #include <cstdio>
      int main() {
        std::atomic<int> state{135};
        for (int i = 1; i <= 10; ++i) {
          int cur = state.load();
          while (!state.compare_exchange_weak(cur, cur + i * 15)) {}
        }
        std::printf("%d\n", state.load());
        return 0;
      }
