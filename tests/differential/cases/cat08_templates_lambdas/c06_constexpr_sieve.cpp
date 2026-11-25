#include <array>
    #include <cstdio>
    constexpr int count_primes_upto_50() {
      std::array<bool, 51> comp{};
      int cnt = 0;
      for (int i = 2; i <= 50; ++i) {
        if (!comp[i]) {
          ++cnt;
          for (int j = i * i; j <= 50; j += i) comp[j] = true;
        }
      }
      return cnt;
    }
    int main() {
      constexpr int n = count_primes_upto_50();
      std::printf("%d\n", n);
      return 0;
    }
