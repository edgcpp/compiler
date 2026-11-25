#include <cstdio>
    template <typename... Ts> constexpr auto sum_all(Ts... xs) {
      return (0 + ... + xs);
    }
    template <typename... Ts> int comma_fold(Ts... xs) {
      int acc = 1; ((acc = acc * 3 + xs), ...); return acc;
    }
    int main() {
      std::printf("%d %d\n", sum_all(1, 2, 3, 4, 5), comma_fold(2, 4, 6));
      return 0;
    }
