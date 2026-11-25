#include <atomic>
    #include <cstdio>
    #include <thread>
    thread_local int tls_val = 7;
    int main() {
      std::atomic<int> sum{0};
      std::thread t1([&]() { tls_val += 10; sum.fetch_add(tls_val); });
      std::thread t2([&]() { tls_val += 20; sum.fetch_add(tls_val); });
      t1.join(); t2.join();
      std::printf("%d:%d\n", tls_val, sum.load());
      return 0;
    }
