#include <concepts>
    #include <cstdio>
    template <std::integral T> int score(T v) { return static_cast<int>(v * 2); }
    template <std::floating_point T> int score(T v) {
      return static_cast<int>(v * 10.0);
    }
    int main() {
      std::printf("%d %d\n", score(21), score(3.5));
      return 0;
    }
