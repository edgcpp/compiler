#include <cstdio>
    static int trace = 0;
    bool bump(int id, bool ret) { trace = trace * 10 + id; return ret; }
    int main() {
      int x = 10, y = 20;
      (true ? x : y) += 5;
      (false ? x : y) += 9;
      bool a = bump(1, false) && bump(2, true);
      bool b = bump(3, true) || bump(4, true);
      bool c = bump(5, true) && bump(6, true);
      std::printf("%d %d %d%d%d %d\n", x, y, a, b, c, trace);
      return 0;
    }
