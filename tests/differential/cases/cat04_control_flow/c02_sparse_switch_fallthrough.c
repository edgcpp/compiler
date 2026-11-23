#include <stdio.h>
    int classify(int v) {
      int r = 0;
      switch (v) {
        case -1000: r += 10; /* fallthrough */
        case 0: r += 5; break;
        case 4096: return 77;
        case 1000000: return 99;
        default: return v + 1;
      }
      return r;
    }
    int main(void) {
      printf("%d %d %d %d %d\n", classify(-1000), classify(0),
             classify(4096), classify(1000000), classify(12));
      return 0;
    }
