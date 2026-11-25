#include <cstdio>
    int main() {
      int ext = 10;
      auto gen = [acc = 5, &ext](auto step) mutable {
        acc += static_cast<int>(step);
        ext += acc;
        return acc;
      };
      int a = gen(3), b = gen(4.0);
      int (*fp)(int) = +[](int x) { return x * x + 1; };
      std::printf("%d %d %d %d\n", a, b, ext, fp(6));
      return 0;
    }
