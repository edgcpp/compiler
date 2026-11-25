#include <cstdio>
    static int steps = 0;
    void inner() {
      try { throw 7; }
      catch (...) { steps = steps * 10 + 1; throw; }
    }
    int main() {
      try { inner(); }
      catch (int x) { steps = steps * 10 + x; }
      std::printf("%d\n", steps);
      return 0;
    }
