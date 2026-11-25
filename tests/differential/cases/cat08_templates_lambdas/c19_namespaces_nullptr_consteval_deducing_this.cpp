// DIFF_STD: -std=c++23
#include <cstddef>
#include <cstdio>
#include <source_location>
namespace outer::inner {
  inline namespace v1 {
    constexpr int kBase = 7;
  }
}
namespace {
  int ovl(int* p) { return p ? *p : -1; }
  int ovl(std::nullptr_t) { return 42; }
}
struct PairNTTP { int a; int b; };
template <PairNTTP P>
consteval int sum_nttp() { return P.a * 10 + P.b; }
struct Fib {
  template <typename Self>
  int calc(this Self&& self, int n) {
    return (n <= 1) ? n : self.calc(n - 1) + self.calc(n - 2);
  }
};
int cur_ln(std::source_location loc = std::source_location::current()) {
  return (int)loc.line();
}
int main() {
  Fib f{};
  int ln = cur_ln();
  std::printf("%d %d %d %d %d\n",
              outer::inner::kBase, ovl(nullptr),
              sum_nttp<PairNTTP{3, 9}>(), f.calc(8), ln);
  return 0;
}
