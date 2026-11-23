#include <cstdio>
      static int tally = 0;
      struct Item { int v; Item(int x):v(x){} ~Item(){ tally += v; } };
      int main() {
        for (int i = 1; i <= 5; ++i) {
          try {
            Item a(i);
            if (i == 3) throw 99;
            tally += 100;
          } catch (int e) {
            tally += e;
          }
        }
        std::printf("%d\n", tally);
        return 0;
      }
