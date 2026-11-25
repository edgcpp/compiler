#include <stdio.h>
    static int stage = 1;
    __attribute__((constructor)) static void init_fn(void) { stage = stage * 10 + 7; }
    int main(void) {
      printf("%d\n", stage);
      return 0;
    }
