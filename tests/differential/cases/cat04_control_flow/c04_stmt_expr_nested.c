#include <stdio.h>
    int main(void) {
      int base = 10;
      int res = ({ int a = base * 2; int b = ({ int t = a + 3; t * 2; }); a + b; });
      printf("%d\n", res);
      return 0;
    }
