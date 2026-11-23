#include <stdio.h>
    static int logv = 0;
    static void clean_int(int *p) { logv = logv * 10 + *p; }
    int run(int flag) {
      __attribute__((cleanup(clean_int))) int a = 1;
      {
        __attribute__((cleanup(clean_int))) int b = 2;
        if (flag) return 9;
      }
      __attribute__((cleanup(clean_int))) int c = 3;
      return 5;
    }
    int main(void) {
      int r1 = run(1), r2 = run(0);
      printf("%d %d %d\n", r1, r2, logv);
      return 0;
    }
