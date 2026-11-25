#include <cstdio>
    #include <tuple>
    #include <type_traits>
    template <typename T> int norm(T x) {
      if constexpr (std::is_pointer_v<T>) return *x + 100;
      else return x + 1;
    }
    struct Pair { int first; int second; };
    int main() {
      int v = 9; Pair p{14, 28}; auto [x, y] = p;
      std::printf("%d %d %d\n", norm(v), norm(&v), x + y);
      return 0;
    }
