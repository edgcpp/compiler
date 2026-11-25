#include <cstdio>
    int risky(int x) { if (x < 0) throw 42; return x * 2; }
    int main() {
      int res = 0;
      try { res += risky(5); res += risky(-1); }
      catch (int e) { res += e; }
      std::printf("%d\n", res);
      return 0;
    }
