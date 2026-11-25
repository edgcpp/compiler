#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
static std::atomic<int> g_static_inits{0};
static std::atomic<int> g_tls_dtors{0};
struct HeavySingleton {
  int val;
  HeavySingleton() : val(77) { g_static_inits.fetch_add(1); }
};
struct TlsWatcher {
  int id = 1;
  ~TlsWatcher() { g_tls_dtors.fetch_add(id); }
};
static int get_singleton() {
  static HeavySingleton s;
  return s.val;
}
int main() {
  std::vector<std::thread> ts;
  int sum = 0;
  std::atomic<int> asum{0};
  for (int i = 0; i < 4; ++i) {
    ts.emplace_back([&asum]() {
      thread_local TlsWatcher w;
      w.id = 3;
      asum.fetch_add(get_singleton());
    });
  }
  for (auto& t : ts) t.join();
  sum = asum.load();
  std::printf("%d %d %d\n", sum, g_static_inits.load(), g_tls_dtors.load());
  return 0;
}
