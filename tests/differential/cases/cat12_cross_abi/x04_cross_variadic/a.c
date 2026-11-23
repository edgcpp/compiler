#include <stdio.h>
    double weighted_var(int n, ...);
    int main(void) {
      double v = weighted_var(4, 2, 1.5, 3, 2.5, 4, 3.5, 5, 4.5);
      printf("%.2f\n", v);
      return 0;
    }
