#include <stdio.h>
    int step(int x) {
      switch (x) {
        case 0: return 3; case 1: return 7; case 2: return 11;
        case 3: return 19; case 4: return 23; case 5: return 29;
        default: return -1;
      }
    }
    int main(void) {
      int s = 0; for (int i = -1; i <= 7; ++i) s = s * 5 + step(i);
      printf("%d\n", s);
      return 0;
    }
