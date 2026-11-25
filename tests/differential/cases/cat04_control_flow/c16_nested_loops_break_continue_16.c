#include <stdio.h>
      int main(void) {
        int total = 0, i = 0;
        while (i < 24) {
          ++i;
          if (i % 3 == 0) continue;
          int j = 0;
          do {
            ++j;
            if (i + j > 27) break;
            total += i * 7 + j;
          } while (j < 16);
        }
        printf("%d\n", total);
        return 0;
      }
