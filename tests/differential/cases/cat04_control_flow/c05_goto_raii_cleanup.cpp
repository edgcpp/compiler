#include <cstdio>
    static int log_v = 0;
    struct Guard {
      int id;
      Guard(int i):id(i){ log_v = log_v*10 + id; }
      ~Guard(){ log_v = log_v*10 + (id + 5); }
    };
    int main() {
      int n = 0;
      { Guard g1(1);
        { Guard g2(2);
          if (++n == 1) goto done;
        }
      }
    done:
      std::printf("%d\n", log_v);
      return 0;
    }
