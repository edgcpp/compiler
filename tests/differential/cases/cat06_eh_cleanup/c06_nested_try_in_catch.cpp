#include <cstdio>
    int main() {
      int total = 0;
      try {
        throw 10;
      } catch (int a) {
        total += a;
        try { throw 25; } catch (int b) { total += b; }
        total += 7;
      }
      std::printf("%d\n", total);
      return 0;
    }
